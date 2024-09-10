//
// Created by William on 8/11/2024.
//


#include "engine.h"

#include <filesystem>

#include "input.h"
#include "../renderer/vk_pipelines.h"
#include "../util/time_utils.h"
#include "draw/draw_structure.h"
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

#include "render_object.h"

#ifdef NDEBUG
#define USE_VALIDATION_LAYERS false
#else
#define USE_VALIDATION_LAYERS true
#endif


VkSampler Engine::defaultSamplerLinear{VK_NULL_HANDLE};
VkSampler Engine::defaultSamplerNearest{VK_NULL_HANDLE};
AllocatedImage Engine::whiteImage{};
AllocatedImage Engine::errorCheckerboardImage{};


void Engine::init()
{
    fmt::print("Initializing Will Engine V2\n");
    auto start = std::chrono::system_clock::now(); {
        // We initialize SDL and create a window with it.
        SDL_Init(SDL_INIT_VIDEO);
        auto window_flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

        window = SDL_CreateWindow(
            "Will Engine V2",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            static_cast<int>(windowExtent.width), // narrowing
            static_cast<int>(windowExtent.height), // narrowing
            window_flags);
    }

    initVulkan();
    initSwapchain();
    initCommands();
    initSyncStructures();


    initDefaultData();

    initDearImgui();

    initScene();

    initPipelines();



    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    fmt::print("Finished Initialization in {} seconds\n\n", static_cast<float>(elapsed.count()) / 1000000.0f);
}

void Engine::run()
{
    fmt::print("Running Will Engine V2\n");

    SDL_Event e;
    bool bQuit = false;
    while (!bQuit) {
        Input& input = Input::Get();
        TimeUtils& time = TimeUtils::Get();
        input.frameReset();

        while (SDL_PollEvent(&e) != 0) {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT) {
                bQuit = true;
            }
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE)
                    bQuit = true;
            }

            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    stopRendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    stopRendering = false;
                }
                if (e.type == SDL_WINDOWEVENT) {
                    if (!resizeRequested) {
                        if (e.window.event == SDL_WINDOWEVENT_RESIZED || e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                            resizeRequested = true;
                            fmt::print("Window resized, resize requested\n");
                        }
                    }
                }
            }

            // imgui input handling
            ImGui_ImplSDL2_ProcessEvent(&e);

            input.processEvent(e);
        }

        input.updateFocus(SDL_GetWindowFlags(window));
        time.update();

        if (resizeRequested) {
            resizeSwapchain();
        }

        // Minimized
        if (stopRendering) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // DearImGui Draw
        {
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            if (ImGui::Begin("Main")) {
                ImGui::Text("Frame Time: %.2f ms", frameTime);
                ImGui::Text("Draw Time: %.2f ms", drawTime);
                ImGui::Text("Delta Time: %.2f ms", time.getDeltaTime() * 1000.0f);
            }
            ImGui::End();

            if (ImGui::Begin("Input Details")) {
                ImGui::Text("Mouse Buttons:");
                ImGui::Columns(4, "MouseButtonsColumns", true);
                ImGui::Separator();
                ImGui::Text("Button");
                ImGui::NextColumn();
                ImGui::Text("Pressed");
                ImGui::NextColumn();
                ImGui::Text("Released");
                ImGui::NextColumn();
                ImGui::Text("State");
                ImGui::NextColumn();
                ImGui::Separator();

                const char* mouseButtons[] = {"LMB", "RMB", "MMB", "M4", "M5"};
                const uint8_t mouseButtonCodes[] = {0, 2, 1, 3, 4};

                for (int i = 0; i < 5; ++i) {
                    ImGui::Text("%s", mouseButtons[i]);
                    ImGui::NextColumn();
                    ImGui::Text("%s", input.isMousePressed(mouseButtonCodes[i]) ? "Yes" : "No");
                    ImGui::NextColumn();
                    ImGui::Text("%s", input.isMouseReleased(mouseButtonCodes[i]) ? "Yes" : "No");
                    ImGui::NextColumn();
                    ImGui::Text("%s", input.isMouseDown(mouseButtonCodes[i]) ? "Down" : "Up");
                    ImGui::NextColumn();
                }

                ImGui::Columns(1);
                ImGui::Separator();

                ImGui::Text("Keyboard:");
                ImGui::Columns(4, "KeyboardColumns", true);
                ImGui::Separator();
                ImGui::Text("Key");
                ImGui::NextColumn();
                ImGui::Text("Pressed");
                ImGui::NextColumn();
                ImGui::Text("Released");
                ImGui::NextColumn();
                ImGui::Text("State");
                ImGui::NextColumn();
                ImGui::Separator();

                const char* keys[] = {"W", "A", "S", "D", "Space", "C", "Left Shift"};
                const SDL_Keycode keyCodes[] = {SDLK_w, SDLK_a, SDLK_s, SDLK_d, SDLK_SPACE, SDLK_c, SDLK_LSHIFT};

                for (int i = 0; i < 7; ++i) {
                    ImGui::Text("%s", keys[i]);
                    ImGui::NextColumn();
                    ImGui::Text("%s", input.isKeyPressed(keyCodes[i]) ? "Yes" : "No");
                    ImGui::NextColumn();
                    ImGui::Text("%s", input.isKeyReleased(keyCodes[i]) ? "Yes" : "No");
                    ImGui::NextColumn();
                    ImGui::Text("%s", input.isKeyDown(keyCodes[i]) ? "Down" : "Up");
                    ImGui::NextColumn();
                }

                ImGui::Columns(1);
                ImGui::Separator();

                ImGui::Text("Mouse Position: (%.1f, %.1f)", input.getMouseX(), input.getMouseY());
                ImGui::Text("Mouse Delta: (%.1f, %.1f)", input.getMouseDeltaX(), input.getMouseDeltaY());
            }
            ImGui::End();

            if (ImGui::Begin("Camera Details")) {
                glm::vec3 viewDir = camera.getViewDirectionWS();
                glm::vec3 cameraPosition = camera.getPosition();
                ImGui::Text("View Direction: (%.2f, %.2f, %.2f)", viewDir.x, viewDir.y, viewDir.z);
                ImGui::Text("Position: (%.2f, %.2f, %.2f)", cameraPosition.x, cameraPosition.y, cameraPosition.z);

                if (ImGui::TreeNode("Matrices")) {
                    glm::mat4 viewMatrix = camera.getViewMatrix();
                    ImGui::Text("View Matrix");
                    for (int i = 0; i < 4; i++)
                        ImGui::Text("%.2f %.2f %.2f %.2f", viewMatrix[i][0], viewMatrix[i][1], viewMatrix[i][2], viewMatrix[i][3]);

                    glm::mat4 projMatrix = camera.getProjMatrix();
                    ImGui::Text("Projection Matrix");
                    for (int i = 0; i < 4; i++)
                        ImGui::Text("%.2f %.2f %.2f %.2f", projMatrix[i][0], projMatrix[i][1], projMatrix[i][2], projMatrix[i][3]);

                    glm::mat4 viewProjMatrix = camera.getViewProjMatrix();
                    ImGui::Text("View-Projection Matrix");
                    for (int i = 0; i < 4; i++)
                        ImGui::Text("%.2f %.2f %.2f %.2f", viewProjMatrix[i][0], viewProjMatrix[i][1], viewProjMatrix[i][2], viewProjMatrix[i][3]);

                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Rotation Matrix")) {
                    glm::mat4 rotationMatrix = camera.getRotationMatrixWS();
                    for (int i = 0; i < 4; i++)
                        ImGui::Text("%.2f %.2f %.2f %.2f", rotationMatrix[i][0], rotationMatrix[i][1], rotationMatrix[i][2], rotationMatrix[i][3]);

                    ImGui::TreePop();
                }
            }
            ImGui::End();

            scene.imguiSceneGraph();

            ImGui::Render();
        }

        if (Input::Get().isKeyPressed(SDLK_p)) {
            GameObject* root = scene.DEBUG_getSceneRoot();

            root->translate(glm::vec3(1.0f, 0.0f, 0.0f));
        }


        draw();
    }
}

void Engine::draw()
{
    camera.update();

    auto start = std::chrono::system_clock::now();

    // GPU -> VPU sync (fence)
    VK_CHECK(vkWaitForFences(device, 1, &getCurrentFrame()._renderFence, true, 1000000000));
    VK_CHECK(vkResetFences(device, 1, &getCurrentFrame()._renderFence));

    // GPU -> GPU sync (semaphore)
    uint32_t swapchainImageIndex;
    VkResult e = vkAcquireNextImageKHR(device, swapchain, 1000000000, getCurrentFrame()._swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR) {
        resizeRequested = true;
        fmt::print("Swapchain out of date or suboptimal, resize requested (At Acquire)\n");
        return;
    }

    // Start Command Buffer Recording
    VkCommandBuffer cmd = getCurrentFrame()._mainCommandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo cmdBeginInfo = vk_helpers::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT); // only submit once
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    drawExtent.height = std::min(swapchainExtent.height, drawImage.imageExtent.height); // * _renderScale;
    drawExtent.width = std::min(swapchainExtent.width, drawImage.imageExtent.width); // * _renderScale;

    // draw compute into _drawImage
    vk_helpers::transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    drawCompute(cmd);

    // draw geometry into _drawImage
    vk_helpers::transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vk_helpers::transitionImage(cmd, depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkClearValue depthClearValue = {0.0f, 0};
    VkRenderingAttachmentInfo colorAttachment = vk_helpers::attachmentInfo(drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = vk_helpers::attachmentInfo(depthImage.imageView, &depthClearValue,
                                                                           VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = vk_helpers::renderingInfo(drawExtent, &colorAttachment, &depthAttachment);
    vkCmdBeginRendering(cmd, &renderInfo);

    drawEnvironment(cmd);
    drawRender(cmd);

    vkCmdEndRendering(cmd);


    // copy Draw Image into Swapchain Image
    vk_helpers::transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vk_helpers::transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vk_helpers::copyImageToImage(cmd, drawImage.image, swapchainImages[swapchainImageIndex], drawExtent, swapchainExtent);

    // draw ImGui into Swapchain Image
    vk_helpers::transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    drawImgui(cmd, swapchainImageViews[swapchainImageIndex]);

    vk_helpers::transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // End Command Buffer Recording
    VK_CHECK(vkEndCommandBuffer(cmd));

    // Submission
    VkCommandBufferSubmitInfo cmdinfo = vk_helpers::commandBufferSubmitInfo(cmd);

    VkSemaphoreSubmitInfo waitInfo = vk_helpers::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                                                     getCurrentFrame()._swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vk_helpers::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, getCurrentFrame()._renderSemaphore);

    VkSubmitInfo2 submit = vk_helpers::submitInfo(&cmdinfo, &signalInfo, &waitInfo);

    //submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submit, getCurrentFrame()._renderFence));


    // Present
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &getCurrentFrame()._renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VkResult presentResult = vkQueuePresentKHR(graphicsQueue, &presentInfo);

    //increase the number of frames drawn
    frameNumber++;

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        resizeRequested = true;
        fmt::print("Swapchain out of date or suboptimal, resize requested (At Present)\n");
    }

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    frameTime = frameTime * 0.99f + elapsed.count() / 1000.0f * 0.01f;
    drawTime = drawTime * 0.99f + elapsed.count() / 1000.0f * 0.01f;
}

void Engine::drawCompute(VkCommandBuffer cmd)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    // Push Constants
    //vkCmdPushConstants(cmd, _backgroundEffectPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &selected._data);


    VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[1]{};
    //descriptor_buffer_binding_info[0] = computeImageDescriptorBuffer.get_descriptor_buffer_binding_info(_device);
    descriptorBufferBindingInfo[0] = computeImageDescriptorBuffer.getDescriptorBufferBindingInfo();
    vkCmdBindDescriptorBuffersEXT(cmd, 1, descriptorBufferBindingInfo);
    uint32_t bufferIndexImage = 0;
    VkDeviceSize bufferOffset = 0;

    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout
                                       , 0, 1, &bufferIndexImage, &bufferOffset);


    // Execute at 8x8 thread groups
    vkCmdDispatch(cmd, std::ceil(drawExtent.width / 16.0), std::ceil(drawExtent.height / 16.0), 1);
}

void Engine::drawEnvironment(VkCommandBuffer cmd)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, environmentPipeline);

    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = drawExtent.width;
    viewport.height = drawExtent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    //  Scissor
    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = drawExtent.width;
    scissor.extent.height = drawExtent.height;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    DescriptorBufferSampler& cubemapSampler = environment->getCubemapDescriptorBuffer();

    VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[1];
    descriptorBufferBindingInfo[0] = cubemapSampler.getDescriptorBufferBindingInfo();
    vkCmdBindDescriptorBuffersEXT(cmd, 1, descriptorBufferBindingInfo);
    uint32_t imageBufferIndex = 0;
    VkDeviceSize imageBufferOffset = 0;

    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, environmentPipelineLayout
                                       , 0, 1, &imageBufferIndex, &imageBufferOffset);


    EnvironmentSceneData sceneData{};
    auto view = glm::lookAt(glm::vec3(0), camera.getViewDirectionWS(), glm::vec3(0, 1, 0));
    auto proj = camera.getProjMatrix();
    sceneData.viewproj = proj * view;
    vkCmdPushConstants(cmd, environmentPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(EnvironmentSceneData), &sceneData);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

void Engine::drawRender(VkCommandBuffer cmd)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderPipeline);

    // Dynamic States
    {
        //  Viewport
        VkViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = drawExtent.width;
        viewport.height = drawExtent.height;
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        //  Scissor
        VkRect2D scissor = {};
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = drawExtent.width;
        scissor.extent.height = drawExtent.height;
        vkCmdSetScissor(cmd, 0, 1, &scissor);
    }

    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = camera.getProjMatrix();
    glm::mat4 mvp = proj * view;
    vkCmdPushConstants(cmd, renderPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mvp);

    testRenderObject->draw(cmd, renderPipelineLayout);
}

void Engine::drawImgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
    VkRenderingAttachmentInfo colorAttachment = vk_helpers::attachmentInfo(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = vk_helpers::renderingInfo(swapchainExtent, &colorAttachment, nullptr);
    vkCmdBeginRendering(cmd, &renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRendering(cmd);
}

void Engine::cleanup()
{
    fmt::print("Cleaning up Will Engine V2\n");

    vkDeviceWaitIdle(device);


    delete environment;
    delete testRenderObject;
    delete testGameObject;
    // destroy all other resources
    mainDeletionQueue.flush();

    // Destroy these after destroying all render objects
    destroyImage(whiteImage);
    destroyImage(errorCheckerboardImage);
    vkDestroySampler(device, defaultSamplerNearest, nullptr);
    vkDestroySampler(device, defaultSamplerLinear, nullptr);

    // ImGui
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(device, imguiPool, nullptr);

    // Main Rendering Command and Fence
    for (auto& frame: frames) {
        vkDestroyCommandPool(device, frame._commandPool, nullptr);

        //destroy sync objects
        vkDestroyFence(device, frame._renderFence, nullptr);
        vkDestroySemaphore(device, frame._renderSemaphore, nullptr);
        vkDestroySemaphore(device, frame._swapchainSemaphore, nullptr);
    }

    // Immediate Command and Fence
    vkDestroyCommandPool(device, immCommandPool, nullptr);
    vkDestroyFence(device, immFence, nullptr);

    // Draw Images
    vkDestroyImageView(device, drawImage.imageView, nullptr);
    vmaDestroyImage(allocator, drawImage.image, drawImage.allocation);
    vkDestroyImageView(device, depthImage.imageView, nullptr);
    vmaDestroyImage(allocator, depthImage.image, depthImage.allocation);
    // Swapchain
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    for (int i = 0; i < swapchainImageViews.size(); i++) {
        vkDestroyImageView(device, swapchainImageViews[i], nullptr);
    }

    // Vulkan Boilerplate
    vmaDestroyAllocator(allocator);

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDevice(device, nullptr);

    vkb::destroy_debug_utils_messenger(instance, debug_messenger);
    vkDestroyInstance(instance, nullptr);

    SDL_DestroyWindow(window);
}

void Engine::initVulkan()
{
    if (VkResult res = volkInitialize(); res != VK_SUCCESS) {
        throw std::runtime_error("Failed to initialize volk");
    }

    vkb::InstanceBuilder builder;

    // make the vulkan instance, with basic debug features
    auto inst_ret = builder.set_app_name("Will's Vulkan Renderer")
            .request_validation_layers(USE_VALIDATION_LAYERS)
            .use_default_debug_messenger()
            .require_api_version(1, 3)
            .build();

    vkb::Instance vkb_inst = inst_ret.value();

    // vulkan instance
    instance = vkb_inst.instance;
    volkLoadInstance(instance);
    debug_messenger = vkb_inst.debug_messenger;

    // sdl vulkan surface
    SDL_Vulkan_CreateSurface(window, instance, &surface);


    // vk 1.3
    VkPhysicalDeviceVulkan13Features features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features.dynamicRendering = true;
    features.synchronization2 = true;
    // vk 1.2
    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    VkPhysicalDeviceFeatures other_features{};
    other_features.multiDrawIndirect = true;
    // Descriptor Buffer Extension
    VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptorBufferFeatures = {};
    descriptorBufferFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
    descriptorBufferFeatures.descriptorBuffer = VK_TRUE;

    // select gpu
    vkb::PhysicalDeviceSelector selector{vkb_inst};
    vkb::PhysicalDevice targetDevice = selector
            .set_minimum_version(1, 3)
            .set_required_features_13(features)
            .set_required_features_12(features12)
            .set_required_features(other_features)
            .add_required_extension(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME)
            .set_surface(surface)
            .select()
            .value();

    vkb::DeviceBuilder deviceBuilder{targetDevice};
    deviceBuilder.add_pNext(&descriptorBufferFeatures);
    vkb::Device vkbDevice = deviceBuilder.build().value();

    device = vkbDevice.device;
    volkLoadDevice(device);
    physicalDevice = targetDevice.physical_device;

    // Graphics Queue
    graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    // VMA
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    allocatorInfo.pVulkanFunctions = &vulkanFunctions;
    vmaCreateAllocator(&allocatorInfo, &allocator);
}

void Engine::initSwapchain()
{
    createSwapchain(windowExtent.width, windowExtent.height);
    createDrawImages(windowExtent.width, windowExtent.height);
}

void Engine::initCommands()
{
    VkCommandPoolCreateInfo commandPoolInfo = vk_helpers::commandPoolCreateInfo(graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (auto& frame: frames) {
        VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &frame._commandPool));
        VkCommandBufferAllocateInfo cmdAllocInfo = vk_helpers::commandBufferAllocateInfo(frame._commandPool);
        VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &frame._mainCommandBuffer));
    }

    // Immediate Rendering
    VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &immCommandPool));
    VkCommandBufferAllocateInfo immCmdAllocInfo = vk_helpers::commandBufferAllocateInfo(immCommandPool);
    VK_CHECK(vkAllocateCommandBuffers(device, &immCmdAllocInfo, &immCommandBuffer));
}

void Engine::initSyncStructures()
{
    VkFenceCreateInfo fenceCreateInfo = vk_helpers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vk_helpers::semaphoreCreateInfo();

    for (auto& frame: frames) {
        VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &frame._renderFence));

        VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frame._swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frame._renderSemaphore));
    }

    // Immediate Rendeirng
    VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &immFence));
}

void Engine::initDefaultData()
{
    // white image
    uint32_t white = packUnorm4x8(glm::vec4(1, 1, 1, 1));
    AllocatedImage newImage = createImage(VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
                                          VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, false);
    destroyImage(newImage);
    whiteImage = createImage((void *) &white, 4, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);


    //checkerboard image
    uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
    uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
    std::array<uint32_t, 16 * 16> pixels{}; //for 16x16 checkerboard texture
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }
    errorCheckerboardImage = createImage(pixels.data(), 16 * 16 * 4, VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampl.magFilter = VK_FILTER_NEAREST;
    sampl.minFilter = VK_FILTER_NEAREST;
    vkCreateSampler(device, &sampl, nullptr, &defaultSamplerNearest);
    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(device, &sampl, nullptr, &defaultSamplerLinear);
}

void Engine::initDearImgui()
{
    // DearImGui implementation, basically copied directly from the Vulkan/SDl2 from DearImGui samples.
    // Because this project uses VOLK, additionally need to load functions.
    // DYNAMIC RENDERING (NOT RENDER PASS)
    VkDescriptorPoolSize pool_sizes[] =
    {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;

    //VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &imguiPool));

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls

    ImGui::StyleColorsLight();

    // Need to LoadFunction when using VOLK/using VK_NO_PROTOTYPES
    ImGui_ImplVulkan_LoadFunctions([](const char* function_name, void* vulkan_instance) {
        return vkGetInstanceProcAddr(*(static_cast<VkInstance *>(vulkan_instance)), function_name);
    }, &instance);

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForVulkan(window);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = instance;
    init_info.PhysicalDevice = physicalDevice;
    init_info.Device = device;
    init_info.QueueFamily = graphicsQueueFamily;
    init_info.Queue = graphicsQueue;
    init_info.DescriptorPool = imguiPool;
    init_info.Subpass = 0;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    //dynamic rendering parameters for imgui to use
    init_info.UseDynamicRendering = true;
    init_info.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchainImageFormat;


    ImGui_ImplVulkan_Init(&init_info);
    ImGui_ImplVulkan_CreateFontsTexture();
}

void Engine::initPipelines()
{
    initComputePipelines();
    initEnvironmentPipeline();
    initRenderPipelines();
}

void Engine::initComputePipelines()
{ {
        DescriptorLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        computeImageDescriptorSetLayout = builder.build(
            device, VK_SHADER_STAGE_COMPUTE_BIT,
            nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );
    }

    VkDescriptorImageInfo drawImageDescriptor{};
    drawImageDescriptor.imageView = drawImage.imageView;
    drawImageDescriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    // needs to match the order of the bindings in the layout
    std::vector<DescriptorImageData> storageImage = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, drawImageDescriptor}
    };

    computeImageDescriptorBuffer = DescriptorBufferSampler(instance, device, physicalDevice, allocator, computeImageDescriptorSetLayout);
    computeImageDescriptorBuffer.setupData(device, storageImage);

    mainDeletionQueue.pushFunction([&]() {
        vkDestroyDescriptorSetLayout(device, computeImageDescriptorSetLayout, nullptr);
        computeImageDescriptorBuffer.destroy(device, allocator);
    });

    // Layout
    //  Descriptors
    VkPipelineLayoutCreateInfo backgroundEffectLayoutCreateInfo{};
    backgroundEffectLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    backgroundEffectLayoutCreateInfo.pNext = nullptr;
    backgroundEffectLayoutCreateInfo.pSetLayouts = &computeImageDescriptorSetLayout;
    backgroundEffectLayoutCreateInfo.setLayoutCount = 1;
    //  Push Constants
    /*VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(ComputePushConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;*/
    backgroundEffectLayoutCreateInfo.pPushConstantRanges = nullptr;
    backgroundEffectLayoutCreateInfo.pushConstantRangeCount = 0;

    VK_CHECK(vkCreatePipelineLayout(device, &backgroundEffectLayoutCreateInfo, nullptr, &computePipelineLayout));

    VkShaderModule gradientShader;
    if (!vk_helpers::loadShaderModule("shaders\\compute.comp.spv", device, &gradientShader)) {
        throw std::runtime_error("Error when building the compute shader (compute.comp.spv)");
    }

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.pNext = nullptr;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = gradientShader;
    stageinfo.pName = "main"; // entry point in shader

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = computePipelineLayout;
    computePipelineCreateInfo.stage = stageinfo;
    computePipelineCreateInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &computePipeline));

    // Cleanup
    vkDestroyShaderModule(device, gradientShader, nullptr);

    mainDeletionQueue.pushFunction([&]() {
        vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
        vkDestroyPipeline(device, computePipeline, nullptr);
    });
}

void Engine::initRenderPipelines()
{
    VkPipelineLayoutCreateInfo layout_info = vk_helpers::pipelineLayoutCreateInfo();
    VkDescriptorSetLayout descriptorLayout[2];
    descriptorLayout[0] = RenderObject::addressesDescriptorSetLayout;
    descriptorLayout[1] = RenderObject::textureDescriptorSetLayout;
    layout_info.pSetLayouts = descriptorLayout;
    layout_info.setLayoutCount = 2;

    VkPushConstantRange pushConstants{};
    pushConstants.offset = 0;
    pushConstants.size = sizeof(glm::mat4);
    pushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layout_info.pPushConstantRanges = &pushConstants;
    layout_info.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(device, &layout_info, nullptr, &renderPipelineLayout));


    VkShaderModule vertShader;
    if (!vk_helpers::loadShaderModule("shaders/cube.vert.spv", device, &vertShader)) {
        throw std::runtime_error("Error when building the cube vertex shader module(cube.vert.spv)\n");
    }
    VkShaderModule fragShader;
    if (!vk_helpers::loadShaderModule("shaders/cube.frag.spv", device, &fragShader)) {
        fmt::print("Error when building the cube fragment shader module(cube.frag.spv)\n");
    }

    PipelineBuilder renderPipelineBuilder; {
        VkVertexInputBindingDescription mainBinding{};
        mainBinding.binding = 0;
        mainBinding.stride = sizeof(Vertex);
        mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;


        VkVertexInputAttributeDescription vertexAttributes[5];
        vertexAttributes[0].binding = 0;
        vertexAttributes[0].location = 0;
        vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        vertexAttributes[0].offset = offsetof(Vertex, position);


        vertexAttributes[1].binding = 0;
        vertexAttributes[1].location = 1;
        vertexAttributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        vertexAttributes[1].offset = offsetof(Vertex, normal);

        vertexAttributes[2].binding = 0;
        vertexAttributes[2].location = 2;
        vertexAttributes[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        vertexAttributes[2].offset = offsetof(Vertex, color);

        vertexAttributes[3].binding = 0;
        vertexAttributes[3].location = 3;
        vertexAttributes[3].format = VK_FORMAT_R32G32_SFLOAT;
        vertexAttributes[3].offset = offsetof(Vertex, uv);

        vertexAttributes[4].binding = 0;
        vertexAttributes[4].location = 4;
        vertexAttributes[4].format = VK_FORMAT_R32_UINT;
        vertexAttributes[4].offset = offsetof(Vertex, materialIndex);

        renderPipelineBuilder.setupVertexInput(&mainBinding, 1, vertexAttributes, 5);
    }

    renderPipelineBuilder.setShaders(vertShader, fragShader);
    renderPipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    renderPipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE); // VK_CULL_MODE_BACK_BIT
    renderPipelineBuilder.disableMultisampling();
    renderPipelineBuilder.setupBlending(PipelineBuilder::BlendMode::NO_BLEND);
    renderPipelineBuilder.enableDepthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    renderPipelineBuilder.setupRenderer(drawImage.imageFormat, depthImage.imageFormat);
    renderPipelineBuilder.setupPipelineLayout(renderPipelineLayout);

    renderPipeline = renderPipelineBuilder.buildPipeline(device, VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    vkDestroyShaderModule(device, vertShader, nullptr);
    vkDestroyShaderModule(device, fragShader, nullptr);

    mainDeletionQueue.pushFunction([&]() {
        vkDestroyPipelineLayout(device, renderPipelineLayout, nullptr);
        vkDestroyPipeline(device, renderPipeline, nullptr);
    });
}

void Engine::initEnvironmentPipeline()
{
    assert(Environment::layoutsCreated);

    const VkDescriptorSetLayout layouts[1] = {Environment::cubemapDescriptorSetLayout};

    VkPipelineLayoutCreateInfo layout_info = vk_helpers::pipelineLayoutCreateInfo();
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = layouts;
    VkPushConstantRange environmentPushConstantRange{};
    environmentPushConstantRange.offset = 0;
    environmentPushConstantRange.size = sizeof(EnvironmentSceneData);
    environmentPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layout_info.pPushConstantRanges = &environmentPushConstantRange;
    layout_info.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(device, &layout_info, nullptr, &environmentPipelineLayout));

    VkShaderModule vertShader;
    if (!vk_helpers::loadShaderModule("shaders/environment/environment.vert.spv", device, &vertShader)) {
        throw std::runtime_error("Error when building the triangle vertex shader module(compute.comp.spv)");
    }
    VkShaderModule fragShader;
    if (!vk_helpers::loadShaderModule("shaders/environment/environment.frag.spv", device, &fragShader)) {
        fmt::print("Error when building the triangle fragment shader module\n");
    }

    PipelineBuilder renderPipelineBuilder;
    renderPipelineBuilder.setShaders(vertShader, fragShader);
    renderPipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    renderPipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    renderPipelineBuilder.disableMultisampling();
    renderPipelineBuilder.setupBlending(PipelineBuilder::BlendMode::NO_BLEND);
    renderPipelineBuilder.enableDepthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    renderPipelineBuilder.setupRenderer(drawImage.imageFormat, depthImage.imageFormat);
    renderPipelineBuilder.setupPipelineLayout(environmentPipelineLayout);

    environmentPipeline = renderPipelineBuilder.buildPipeline(device, VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    vkDestroyShaderModule(device, vertShader, nullptr);
    vkDestroyShaderModule(device, fragShader, nullptr);

    mainDeletionQueue.pushFunction([&]() {
        vkDestroyPipelineLayout(device, environmentPipelineLayout, nullptr);
        vkDestroyPipeline(device, environmentPipeline, nullptr);
    });
}

void Engine::initScene()
{
    environment = new Environment(this);
    environment->loadCubemap(Environment::defaultEquiPath, 0);

    //testRenderObject = new RenderObject{this, "assets/models/BoxTextured/glTF/BoxTextured.gltf"};
    //testRenderObject = new RenderObject{this, "assets/models/structure_mat.glb"};
    testRenderObject = new RenderObject{this, "assets/models/structure.glb"};
    //testRenderObject = new RenderObject{this, "assets/models/Suzanne/glTF/Suzanne.gltf"};
    //testRenderObject = new RenderObject{this, "assets/models/glTF/Sponza.gltf"};
    testGameObject = testRenderObject->GenerateGameObject();
    scene.addGameObject(testGameObject);
}

void Engine::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function) const
{
    VK_CHECK(vkResetFences(device, 1, &immFence));
    VK_CHECK(vkResetCommandBuffer(immCommandBuffer, 0));

    VkCommandBuffer cmd = immCommandBuffer;
    VkCommandBufferBeginInfo cmdBeginInfo = vk_helpers::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
    function(cmd);
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdSubmitInfo = vk_helpers::commandBufferSubmitInfo(cmd);
    VkSubmitInfo2 submitInfo = vk_helpers::submitInfo(&cmdSubmitInfo, nullptr, nullptr);

    VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submitInfo, immFence));

    VK_CHECK(vkWaitForFences(device, 1, &immFence, true, 1000000000));
}

AllocatedBuffer Engine::createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) const
{
    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memoryUsage;
    vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    vmaallocInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    AllocatedBuffer newBuffer{};

    VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

    return newBuffer;
}

AllocatedBuffer Engine::createStagingBuffer(size_t allocSize) const
{
    return createBuffer(allocSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
}

void Engine::copyBuffer(const AllocatedBuffer& src, const AllocatedBuffer& dst, const VkDeviceSize size) const
{
    immediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{0};
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = size;

        vkCmdCopyBuffer(cmd, src.buffer, dst.buffer, 1, &vertexCopy);
    });
}

VkDeviceAddress Engine::getBufferAddress(const AllocatedBuffer& buffer) const
{
    VkBufferDeviceAddressInfo addressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR};
    addressInfo.buffer = buffer.buffer;
    VkDeviceAddress srcPtr = vkGetBufferDeviceAddress(device, &addressInfo);
    return srcPtr;
}

void Engine::destroyBuffer(const AllocatedBuffer& buffer) const
{
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

AllocatedImage Engine::createImage(const VkExtent3D size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped) const
{
    AllocatedImage newImage{};
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo imgInfo = vk_helpers::imageCreateInfo(format, usage, size);
    if (mipmapped) {
        imgInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocinfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    VK_CHECK(vmaCreateImage(allocator, &imgInfo, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

    // if the format is a depth format, we will need to have it use the correct
    // aspect flag
    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT) {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    // build a image-view for the image
    VkImageViewCreateInfo view_info = vk_helpers::imageviewCreateInfo(format, newImage.image, aspectFlag);
    view_info.subresourceRange.levelCount = imgInfo.mipLevels;

    VK_CHECK(vkCreateImageView(device, &view_info, nullptr, &newImage.imageView));

    return newImage;
}

AllocatedImage Engine::createImage(const void* data, size_t dataSize, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                                   bool mipmapped /*= false*/) const
{
    size_t data_size = dataSize;
    AllocatedBuffer uploadbuffer = createBuffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    memcpy(uploadbuffer.info.pMappedData, data, data_size);

    AllocatedImage newImage = createImage(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

    immediateSubmit([&](VkCommandBuffer cmd) {
        vk_helpers::transitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = size;

        // copy the buffer into the image
        vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        if (mipmapped) {
            vk_helpers::generateMipmaps(cmd, newImage.image, VkExtent2D{newImage.imageExtent.width, newImage.imageExtent.height});
        } else {
            vk_helpers::transitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    });

    destroyBuffer(uploadbuffer);

    return newImage;
}

AllocatedImage Engine::createCubemap(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    AllocatedImage newImage{};
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo img_info = vk_helpers::cubemapCreateInfo(format, usage, size);
    if (mipmapped) {
        img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }
    // always allocate images on dedicated GPU memory
    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vmaCreateImage(allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

    VkImageViewCreateInfo view_info = vk_helpers::cubemapViewCreateInfo(format, newImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
    view_info.subresourceRange.levelCount = img_info.mipLevels;

    VK_CHECK(vkCreateImageView(device, &view_info, nullptr, &newImage.imageView));

    return newImage;
}

int Engine::getChannelCount(VkFormat format)
{
    switch (format) {
        case VK_FORMAT_R8G8B8A8_UNORM:
            return 4;
        case VK_FORMAT_R8G8B8_UNORM:
            return 3;
        case VK_FORMAT_R8_UNORM:
            return 1;
        default:
            return 0;
    }
}

void Engine::destroyImage(const AllocatedImage& img) const
{
    vkDestroyImageView(device, img.imageView, nullptr);
    vmaDestroyImage(allocator, img.image, img.allocation);
}

void Engine::createSwapchain(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchainBuilder{physicalDevice, device, surface};

    swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

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
    vkDeviceWaitIdle(device);

    vkDestroySwapchainKHR(device, swapchain, nullptr);
    for (int i = 0; i < swapchainImageViews.size(); i++) {
        vkDestroyImageView(device, swapchainImageViews[i], nullptr);
    }

    int w, h;
    // get new window size
    SDL_GetWindowSize(window, &w, &h);
    windowExtent.width = w;
    windowExtent.height = h;

    /*_maxRenderScale = std::min(
        (float)_drawImage.imageExtent.width / (float)_windowExtent.width
        , (float)_drawImage.imageExtent.height / (float)_windowExtent.height
    );*/
    //_maxRenderScale = std::max(_maxRenderScale, 1.0f);

    //_renderScale = std::min(_maxRenderScale, _renderScale);

    createSwapchain(windowExtent.width, windowExtent.height);

    resizeRequested = false;
    fmt::print("Window extent has been updated to {}x{}\n", windowExtent.width, windowExtent.height);
}

void Engine::createDrawImages(uint32_t width, uint32_t height)
{
    // Draw Image
    drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkExtent3D drawImageExtent = {width, height, 1};
    drawImage.imageExtent = drawImageExtent;
    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
    VkImageCreateInfo rimg_info = vk_helpers::imageCreateInfo(drawImage.imageFormat, drawImageUsages, drawImageExtent);

    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vmaCreateImage(allocator, &rimg_info, &rimg_allocinfo, &drawImage.image, &drawImage.allocation, nullptr);

    VkImageViewCreateInfo rview_info = vk_helpers::imageviewCreateInfo(drawImage.imageFormat, drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
    VK_CHECK(vkCreateImageView(device, &rview_info, nullptr, &drawImage.imageView));


    // Depth Image
    depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    VkExtent3D depthImageExtent = {width, height, 1};
    depthImage.imageExtent = depthImageExtent;
    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    VkImageCreateInfo dimg_info = vk_helpers::imageCreateInfo(depthImage.imageFormat, depthImageUsages, depthImageExtent);

    VmaAllocationCreateInfo dimg_allocinfo = {};
    dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    dimg_allocinfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vmaCreateImage(allocator, &dimg_info, &dimg_allocinfo, &depthImage.image, &depthImage.allocation, nullptr);

    VkImageViewCreateInfo dview_info = vk_helpers::imageviewCreateInfo(depthImage.imageFormat, depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);
    VK_CHECK(vkCreateImageView(device, &dview_info, nullptr, &depthImage.imageView));

    drawExtent = {width, height};
}
