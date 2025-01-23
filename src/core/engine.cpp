//
// Created by William on 8/11/2024.
//

#include "engine.h"

#include <thread>


#include <VkBootstrap.h>

#include "src/core/input.h"
#include "src/core/time_utils.h"
#include "src/renderer/immediate_submitter.h"
#include "src/renderer/resource_manager.h"
#include "src/renderer/vk_descriptors.h"

#ifdef NDEBUG
#define USE_VALIDATION_LAYERS false
#else
#define USE_VALIDATION_LAYERS true
#endif

using will_engine::Time;
using will_engine::Input;

void Engine::init()
{
    fmt::print("----------------------------------------\n");
    fmt::print("Initializing {}\n", ENGINE_NAME);
    auto start = std::chrono::system_clock::now();
    //
    {
        // We initialize SDL and create a window with it.
        SDL_Init(SDL_INIT_VIDEO);
        auto window_flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

        window = SDL_CreateWindow(
            ENGINE_NAME,
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            static_cast<int>(windowExtent.width), // narrowing
            static_cast<int>(windowExtent.height), // narrowing
            window_flags);
    }

    context = new VulkanContext(window, USE_VALIDATION_LAYERS);

    createSwapchain(windowExtent.width, windowExtent.height);
    createDrawResources(RENDER_EXTENTS.width, RENDER_EXTENTS.height);

    // Command Pools
    VkCommandPoolCreateInfo commandPoolInfo = vk_helpers::commandPoolCreateInfo(context->graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    for (auto& frame : frames) {
        VK_CHECK(vkCreateCommandPool(context->device, &commandPoolInfo, nullptr, &frame._commandPool));
        VkCommandBufferAllocateInfo cmdAllocInfo = vk_helpers::commandBufferAllocateInfo(frame._commandPool);
        VK_CHECK(vkAllocateCommandBuffers(context->device, &cmdAllocInfo, &frame._mainCommandBuffer));
    }

    // Sync Structures
    const VkFenceCreateInfo fenceCreateInfo = vk_helpers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    const VkSemaphoreCreateInfo semaphoreCreateInfo = vk_helpers::semaphoreCreateInfo();
    for (auto& frame : frames) {
        VK_CHECK(vkCreateFence(context->device, &fenceCreateInfo, nullptr, &frame._renderFence));

        VK_CHECK(vkCreateSemaphore(context->device, &semaphoreCreateInfo, nullptr, &frame._swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(context->device, &semaphoreCreateInfo, nullptr, &frame._renderSemaphore));
    }

    immediate = new ImmediateSubmitter(*context);
    resourceManager = new ResourceManager(*context, *immediate);
    // physics = new Physics();
    // Physics::Set(physics);

    imguiWrapper = new ImguiWrapper(*context, {window, swapchainImageFormat});

    //
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        sceneDataLayout = layoutBuilder.build(context->device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, nullptr,
                                              VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

        for (int i{0}; i < FRAME_OVERLAP; i++) {
            sceneDataBuffers[i] = resourceManager->createHostSequentialUniformBuffer(sizeof(SceneData));
        }


        sceneDataDescriptorBuffer = DescriptorBufferUniform(*context, sceneDataLayout, FRAME_OVERLAP);

        std::vector<DescriptorUniformData> sceneDataBufferData{1};
        for (int i{0}; i < FRAME_OVERLAP; i++) {
            sceneDataBufferData[0] = DescriptorUniformData{.uniformBuffer = sceneDataBuffers[i], .allocSize = sizeof(SceneData)};
            sceneDataDescriptorBuffer.setupData(context->device, sceneDataBufferData, i);
        }
    }

    computePipeline = new basic_compute::BasicComputePipeline(*context);
    computePipeline->setupDescriptors({drawImage.imageView});
    renderPipeline = new basic_render::BasicRenderPipeline({drawImageFormat, depthImageFormat, sceneDataLayout}, *context);
    renderPipeline->setupDescriptors({resourceManager->getDefaultSamplerNearest(), resourceManager->getErrorCheckerboardImage().imageView});

    const auto end = std::chrono::system_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    fmt::print("Finished Initialization in {} seconds\n\n", static_cast<float>(elapsed.count()) / 1000000.0f);
}

void Engine::run()
{
    fmt::print("----------------------------------------\n");
    fmt::print("Running {}\n", ENGINE_NAME);

    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
        Input& input = Input::Get();
        Time& time = Time::Get();
        input.frameReset();

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) { bQuit = true; }
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) { bQuit = true; }

            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) { bStopRendering = true; }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) { bStopRendering = false; }
                if (e.type == SDL_WINDOWEVENT) {
                    if (!bResizeRequested) {
                        if (e.window.event == SDL_WINDOWEVENT_RESIZED || e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                            bResizeRequested = true;
                            fmt::print("Window resized, resize requested\n");
                        }
                    }
                }
            }

            imguiWrapper->handleInput(e);
            input.processEvent(e);
        }

        input.updateFocus(SDL_GetWindowFlags(window));
        time.update();

        if (bResizeRequested) {
            resizeSwapchain();
        }

        // Minimized
        if (bStopRendering) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        imguiWrapper->imguiInterface(this);

        draw();
    }
}

void Engine::draw()
{
    const auto start = std::chrono::system_clock::now();

    // GPU -> VPU sync (fence)
    VK_CHECK(vkWaitForFences(context->device, 1, &getCurrentFrame()._renderFence, true, 1000000000));
    VK_CHECK(vkResetFences(context->device, 1, &getCurrentFrame()._renderFence));

    // GPU -> GPU sync (semaphore)
    uint32_t swapchainImageIndex;
    VkResult e = vkAcquireNextImageKHR(context->device, swapchain, 1000000000, getCurrentFrame()._swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR) {
        bResizeRequested = true;
        fmt::print("Swapchain out of date or suboptimal, resize requested (At Acquire)\n");
        return;
    }

    VkCommandBuffer cmd = getCurrentFrame()._mainCommandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    const VkCommandBufferBeginInfo cmdBeginInfo = vk_helpers::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT); // only submit once
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));


    const AllocatedBuffer& sceneDataBuffer = sceneDataBuffers[getCurrentFrameOverlap()];
    const auto pSceneData = static_cast<SceneData*>(sceneDataBuffer.info.pMappedData);
    pSceneData->currentFrame = frameNumber;

    vk_helpers::transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
    computePipeline->draw(cmd, {RENDER_EXTENTS});

    // draw geometry into _drawImage
    vk_helpers::transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
    renderPipeline->draw(cmd, {RENDER_EXTENTS, drawImage.imageView, depthImage.imageView, sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo(), sceneDataDescriptorBuffer.getDescriptorBufferSize() * getCurrentFrameOverlap(), frameNumber});

    // copy Draw Image into Swapchain Image
    vk_helpers::transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::copyImageToImage(cmd, drawImage.image, swapchainImages[swapchainImageIndex], RENDER_EXTENTS, swapchainExtent);

    // draw ImGui into Swapchain Image
    vk_helpers::transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    imguiWrapper->drawImgui(cmd, swapchainImageViews[swapchainImageIndex], swapchainExtent);

    vk_helpers::transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_ASPECT_COLOR_BIT);

    // End Command Buffer Recording
    VK_CHECK(vkEndCommandBuffer(cmd));

    // Submission
    const VkCommandBufferSubmitInfo cmdSubmitInfo = vk_helpers::commandBufferSubmitInfo(cmd);
    const VkSemaphoreSubmitInfo waitInfo = vk_helpers::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, getCurrentFrame()._swapchainSemaphore);
    const VkSemaphoreSubmitInfo signalInfo = vk_helpers::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, getCurrentFrame()._renderSemaphore);
    const VkSubmitInfo2 submit = vk_helpers::submitInfo(&cmdSubmitInfo, &signalInfo, &waitInfo);

    //submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(context->graphicsQueue, 1, &submit, getCurrentFrame()._renderFence));


    // Present
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &getCurrentFrame()._renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VkResult presentResult = vkQueuePresentKHR(context->graphicsQueue, &presentInfo);

    //increase the number of frames drawn
    frameNumber++;

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        bResizeRequested = true;
        fmt::print("Swapchain out of date or suboptimal, resize requested (At Present)\n");
    }

    const auto end = std::chrono::system_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    stats.renderTime = stats.renderTime * 0.99f + static_cast<float>(elapsed.count()) / 1000.0f * 0.01f;
    stats.totalTime = stats.renderTime + stats.gameTime + stats.physicsTime;
}

void Engine::cleanup()
{
    fmt::print("Cleaning up Will Engine V2\n");

    vkDeviceWaitIdle(context->device);

    delete computePipeline;
    delete renderPipeline;

    for (AllocatedBuffer sceneBuffer : sceneDataBuffers) {
        resourceManager->destroyBuffer(sceneBuffer);
    }
    vkDestroyDescriptorSetLayout(context->device, sceneDataLayout, nullptr);
    sceneDataDescriptorBuffer.destroy(context->allocator);

    delete imguiWrapper;

    for (const FrameData& frame : frames) {
        vkDestroyCommandPool(context->device, frame._commandPool, nullptr);

        //destroy sync objects
        vkDestroyFence(context->device, frame._renderFence, nullptr);
        vkDestroySemaphore(context->device, frame._renderSemaphore, nullptr);
        vkDestroySemaphore(context->device, frame._swapchainSemaphore, nullptr);
    }

    resourceManager->destroyImage(drawImage);
    resourceManager->destroyImage(depthImage);

    delete immediate;
    delete resourceManager;


    vkDestroySwapchainKHR(context->device, swapchain, nullptr);
    for (VkImageView swapchainImageView : swapchainImageViews) {
        vkDestroyImageView(context->device, swapchainImageView, nullptr);
    }

    delete context;

    SDL_DestroyWindow(window);
}

void Engine::createSwapchain(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchainBuilder{context->physicalDevice, context->device, context->surface};

    swapchainImageFormat = VK_FORMAT_R8G8B8A8_UNORM;

    vkb::Swapchain vkbSwapchain = swapchainBuilder
            .set_desired_format(VkSurfaceFormatKHR{.format = swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR) //use vsync present mode
            .set_desired_extent(width, height)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build()
            .value();

    swapchainExtent = vkbSwapchain.extent;

    // Swapchain and SwapchainImages
    swapchain = vkbSwapchain.swapchain;
    swapchainImages = vkbSwapchain.get_images().value();
    swapchainImageViews = vkbSwapchain.get_image_views().value();
}

void Engine::resizeSwapchain()
{
    vkDeviceWaitIdle(context->device);

    vkDestroySwapchainKHR(context->device, swapchain, nullptr);
    for (const auto swapchainImage : swapchainImageViews) {
        vkDestroyImageView(context->device, swapchainImage, nullptr);
    }

    int w, h;
    // get new window size
    SDL_GetWindowSize(window, &w, &h);
    windowExtent.width = w;
    windowExtent.height = h;

    createSwapchain(windowExtent.width, windowExtent.height);

    bResizeRequested = false;
    fmt::print("Window extent has been updated to {}x{}\n", windowExtent.width, windowExtent.height);
}

void Engine::createDrawResources(uint32_t width, uint32_t height)
{
    // Draw Image
    {
        drawImage.imageFormat = drawImageFormat;
        VkExtent3D drawImageExtent = {width, height, 1};
        drawImage.imageExtent = drawImageExtent;
        VkImageUsageFlags drawImageUsages{};
        drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
        VkImageCreateInfo renderImageInfo = vk_helpers::imageCreateInfo(drawImage.imageFormat, drawImageUsages, drawImageExtent);

        VmaAllocationCreateInfo renderImageAllocationInfo = {};
        renderImageAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        renderImageAllocationInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vmaCreateImage(context->allocator, &renderImageInfo, &renderImageAllocationInfo, &drawImage.image, &drawImage.allocation, nullptr);

        VkImageViewCreateInfo renderViewInfo = vk_helpers::imageviewCreateInfo(drawImage.imageFormat, drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
        VK_CHECK(vkCreateImageView(context->device, &renderViewInfo, nullptr, &drawImage.imageView));
    }
    // Depth Image
    {
        depthImage.imageFormat = depthImageFormat;
        VkExtent3D depthImageExtent = {width, height, 1};
        depthImage.imageExtent = depthImageExtent;
        VkImageUsageFlags depthImageUsages{};
        depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depthImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        depthImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
        VkImageCreateInfo depthImageInfo = vk_helpers::imageCreateInfo(depthImage.imageFormat, depthImageUsages, depthImageExtent);

        VmaAllocationCreateInfo depthImageAllocationInfo = {};
        depthImageAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        depthImageAllocationInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vmaCreateImage(context->allocator, &depthImageInfo, &depthImageAllocationInfo, &depthImage.image, &depthImage.allocation, nullptr);

        VkImageViewCreateInfo depthViewInfo = vk_helpers::imageviewCreateInfo(depthImage.imageFormat, depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);
        VK_CHECK(vkCreateImageView(context->device, &depthViewInfo, nullptr, &depthImage.imageView));
    }
}
