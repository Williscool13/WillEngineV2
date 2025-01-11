//
// Created by William on 8/11/2024.
//


#include "engine.h"

#include <cmath>
#include <filesystem>

#include "input.h"

#include "imgui_wrapper.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "src/game/camera/free_camera.h"
#include "src/game/player/player_character.h"
#include "src/util/halton.h"
#include "src/util/time_utils.h"

#include "src/renderer/immediate_submitter.h"
#include "src/renderer/resource_manager.h"
#include "src/renderer/vk_descriptors.h"
#include "src/renderer/environment/environment.h"
#include "src/renderer/renderer_constants.h"
#include "src/renderer/pipelines/acceleration_algorithms/frustum_culling_descriptor_layout.h"
#include "src/renderer/pipelines/acceleration_algorithms/frustum_culling_pipeline.h"
#include "src/renderer/pipelines/environment/environment_descriptor_layouts.h"
#include "src/renderer/pipelines/environment/environment_map_pipeline.h"
#include "src/renderer/pipelines/deferred/deferred_mrt.h"
#include "src/renderer/pipelines/deferred/deferred_resolve.h"
#include "src/renderer/pipelines/post_processing/temporal_antialiasing.h"
#include "src/renderer/pipelines/post_processing/post_process.h"
#include "src/renderer/render_object/render_object_descriptor_layout.h"
#include "src/renderer/render_object/render_object.h"
#include "src/renderer/scene/scene_descriptor_layouts.h"

#include "src/physics/physics.h"
#include "src/physics/physics_filters.h"
#include "src/physics/physics_utils.h"
#include "src/renderer/environment/environment_map_types.h"
#include "src/renderer/lighting/shadows/cascaded_shadow_map.h"
#include "src/renderer/lighting/shadows/cascaded_shadow_map_descriptor_layouts.h"
#include "src/renderer/lighting/shadows/shadow_types.h"
#include "src/renderer/pipelines/acceleration_algorithms/frustum_culling_types.h"
#include "src/util/render_utils.h"

#ifdef NDEBUG
#define USE_VALIDATION_LAYERS false
#else
#define USE_VALIDATION_LAYERS true
#endif

VkDescriptorSetLayout Engine::emptyDescriptorSetLayout;

void Engine::init()
{
    fmt::print("----------------------------------------\n");
    fmt::print("Initializing Will Engine V2\n");
    const auto start = std::chrono::system_clock::now();

    //
    {
        // We initialize SDL and create a window with it.
        SDL_Init(SDL_INIT_VIDEO);
        constexpr auto window_flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE);
        windowExtent = {1920, 1080};

        //constexpr auto window_flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);


        window = SDL_CreateWindow(
            ENGINE_NAME,
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            static_cast<int>(windowExtent.width),
            static_cast<int>(windowExtent.height),
            window_flags);

        if (SDL_Surface* icon = SDL_LoadBMP("assets/icons/WillEngine.bmp")) {
            SDL_SetWindowIcon(window, icon);
            SDL_FreeSurface(icon);
        } else {
            fmt::print("Failed to load window icon: {}\n", SDL_GetError());
        }
    }

    context = new VulkanContext(window, true);

    createSwapchain(windowExtent.width, windowExtent.height);
    createDrawResources(renderExtent.width, renderExtent.height);


    // Command Pools
    {
        const VkCommandPoolCreateInfo commandPoolInfo = vk_helpers::commandPoolCreateInfo(context->graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

        for (auto& frame : frames) {
            VK_CHECK(vkCreateCommandPool(context->device, &commandPoolInfo, nullptr, &frame._commandPool));
            VkCommandBufferAllocateInfo cmdAllocInfo = vk_helpers::commandBufferAllocateInfo(frame._commandPool);
            VK_CHECK(vkAllocateCommandBuffers(context->device, &cmdAllocInfo, &frame._mainCommandBuffer));
        }
    }

    // Sync Structures
    {
        const VkFenceCreateInfo fenceCreateInfo = vk_helpers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
        const VkSemaphoreCreateInfo semaphoreCreateInfo = vk_helpers::semaphoreCreateInfo();

        for (auto& frame : frames) {
            VK_CHECK(vkCreateFence(context->device, &fenceCreateInfo, nullptr, &frame._renderFence));

            VK_CHECK(vkCreateSemaphore(context->device, &semaphoreCreateInfo, nullptr, &frame._swapchainSemaphore));
            VK_CHECK(vkCreateSemaphore(context->device, &semaphoreCreateInfo, nullptr, &frame._renderSemaphore));
        }
    }

    immediate = new ImmediateSubmitter(*context);
    resourceManager = new ResourceManager(*context, *immediate);
    physics = new Physics();
    Physics::Set(physics);

    environmentDescriptorLayouts = new EnvironmentDescriptorLayouts(*context);
    shadowMapDescriptorLayouts = new CascadedShadowMapDescriptorLayouts(*context);
    sceneDescriptorLayouts = new SceneDescriptorLayouts(*context);
    frustumCullDescriptorLayouts = new FrustumCullingDescriptorLayouts(*context);
    renderObjectDescriptorLayout = new RenderObjectDescriptorLayout(*context);

    imguiWrapper = new ImguiWrapper(*context, {window, swapchainImageFormat});

    initScene();

    initRenderer();


    const auto end = std::chrono::system_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    fmt::print("Finished Initialization in {} seconds\n", static_cast<float>(elapsed.count()) / 1000000.0f);
}

void Engine::run()
{
    fmt::print("----------------------------------------\n");
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
                    bStopRendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    bStopRendering = false;
                }
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

void Engine::updateSceneData(VkCommandBuffer cmd) const
{
    const bool bIsFrameZero = frameNumber == 0;

    glm::vec2 prevJitter = HaltonSequence::getJitterHardcoded(bIsFrameZero ? frameNumber : frameNumber - 1) * 2.0f - 1.0f;
    prevJitter.x /= static_cast<float>(renderExtent.width);
    prevJitter.y /= static_cast<float>(renderExtent.height);
    glm::vec2 currentJitter = HaltonSequence::getJitterHardcoded(frameNumber) * 2.0f - 1.0f;
    currentJitter.x /= static_cast<float>(renderExtent.width);
    currentJitter.y /= static_cast<float>(renderExtent.height);

    const Camera* camera = player->getCamera();

    // Update scene data
    {
        const auto pSceneData = static_cast<SceneData*>(sceneDataBuffer.info.pMappedData);

        pSceneData->prevView = bIsFrameZero ? camera->getViewMatrix() : pSceneData->view;
        pSceneData->prevProj = bIsFrameZero ? camera->getProjMatrix() : pSceneData->proj;
        pSceneData->prevViewProj = bIsFrameZero ? camera->getViewProjMatrix() : pSceneData->viewProj;
        pSceneData->jitter = bEnableJitter ? glm::vec4(currentJitter.x, currentJitter.y, prevJitter.x, prevJitter.y) : glm::vec4(0.0f);

        pSceneData->view = camera->getViewMatrix();
        pSceneData->proj = camera->getProjMatrix();
        pSceneData->viewProj = pSceneData->proj * pSceneData->view;

        if (bEnableShadowMapDebug) {
            const glm::mat4 lightViewProj = cascadedShadowMap->getCascadeViewProjection(mainLight.getDirection(), camera->getCameraProperties(), shadowMapDebug);
            pSceneData->viewProj = lightViewProj;
        }

        pSceneData->invView = glm::inverse(pSceneData->view);
        pSceneData->invProj = glm::inverse(pSceneData->proj);
        pSceneData->invViewProj = glm::inverse(pSceneData->viewProj);
        pSceneData->cameraWorldPos = camera->getPosition();
        const glm::mat4 cameraLook = glm::lookAt(glm::vec3(0), camera->getForwardWS(), glm::vec3(0, 1, 0));
        pSceneData->viewProjCameraLookDirection = pSceneData->proj * cameraLook;

        pSceneData->renderTargetSize = {renderExtent.width, renderExtent.height};
        pSceneData->deltaTime = TimeUtils::Get().getDeltaTime();

        VkBufferMemoryBarrier2 barrier{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
            .srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
            .dstAccessMask = VK_ACCESS_2_UNIFORM_READ_BIT,
            .buffer = sceneDataBuffer.buffer,
            .offset = 0,
            .size = sizeof(SceneData)
        };
        const VkDependencyInfo dependencyInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers = &barrier
        };
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    }
}


void Engine::draw()
{
    const auto start = std::chrono::system_clock::now();

    const float deltaTime = TimeUtils::Get().getDeltaTime();

    const auto physicsStart = std::chrono::system_clock::now();
    physics->update(deltaTime);
    const auto physicsEnd = std::chrono::system_clock::now();
    const float elapsedPhysics = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(physicsEnd - physicsStart).count()) / 1000.0f;
    physicsTime = physicsTime * 0.99 + elapsedPhysics * 0.01f;

    const auto gameStart = std::chrono::system_clock::now();
    update(deltaTime);
    const auto gameEnd = std::chrono::system_clock::now();
    const float elapsedGame = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(gameEnd - gameStart).count()) / 1000.0f;
    gameTime = gameTime * 0.99 + elapsedGame * 0.01f;

#pragma region Fence / Swapchain
    // GPU -> CPU sync (fence)
    VK_CHECK(vkWaitForFences(context->device, 1, &getCurrentFrame()._renderFence, true, 1000000000));
    VK_CHECK(vkResetFences(context->device, 1, &getCurrentFrame()._renderFence));

    // GPU -> GPU sync (semaphore)
    uint32_t swapchainImageIndex;
    const VkResult e = vkAcquireNextImageKHR(context->device, swapchain, 1000000000, getCurrentFrame()._swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR) {
        bResizeRequested = true;
        fmt::print("Swapchain out of date or suboptimal, resize requested (At Acquire)\n");
        return;
    }
#pragma endregion

    const auto cmd = getCurrentFrame()._mainCommandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    const VkCommandBufferBeginInfo cmdBeginInfo = vk_helpers::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));


    const auto renderStart = std::chrono::system_clock::now();
    scene.updateSceneModelMatrices();
    updateSceneData(cmd);
    cascadedShadowMap->updateCascadeData();


    const std::vector renderObjects{sponza, cube, primitives};

    const FrustumCullDrawInfo shadowPassFrustumCullingDrawInfo = {
        renderObjects,
        sceneDataDescriptorBuffer,
        false
    };
    frustumCullingPipeline->draw(cmd, shadowPassFrustumCullingDrawInfo);


    const CascadedShadowMapDrawInfo shadowMapDrawInfo{
        renderObjects,
        player->getCamera()->getCameraProperties(),
        mainLight,
    };
    cascadedShadowMap->draw(cmd, shadowMapDrawInfo);


    const FrustumCullDrawInfo frustumCullingDrawInfo{
        renderObjects,
        sceneDataDescriptorBuffer,
        bEnableFrustumCulling
    };
    frustumCullingPipeline->draw(cmd, frustumCullingDrawInfo);

    vk_helpers::transitionImage(cmd, depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
    vk_helpers::transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

    const EnvironmentDrawInfo environmentDrawInfo{
        renderExtent,
        drawImage.imageView,
        depthImage.imageView,
        sceneDataDescriptorBuffer,
        sceneDataDescriptorBuffer.getDescriptorBufferSize() * 0,
        environmentMap->getCubemapDescriptorBuffer(),
        environmentMap->getEnvironmentMapOffset(environmentMapIndex)
    };
    environmentPipeline->draw(cmd, environmentDrawInfo);

    vk_helpers::transitionImage(cmd, normalRenderTarget.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, albedoRenderTarget.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, pbrRenderTarget.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, velocityRenderTarget.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

    DeferredMrtDrawInfo deferredMrtDrawInfo{
        renderObjects,
        normalRenderTarget.imageView,
        albedoRenderTarget.imageView,
        pbrRenderTarget.imageView,
        velocityRenderTarget.imageView,
        depthImage.imageView,
        renderExtent,
        {static_cast<float>(renderExtent.width), static_cast<float>(renderExtent.height)},
        sceneDataDescriptorBuffer,
    };

    deferredMrtPipeline->draw(cmd, deferredMrtDrawInfo);

    vk_helpers::transitionImage(cmd, normalRenderTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, albedoRenderTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, pbrRenderTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, velocityRenderTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, depthImage.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
    vk_helpers::transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

    DeferredResolveDrawInfo deferredResolveDrawInfo{
        renderExtent,
        deferredDebug,
        sceneDataDescriptorBuffer,
        sceneDataDescriptorBuffer.getDescriptorBufferSize() * 0,
        environmentMap,
        environmentMap->getDiffSpecMapOffset(environmentMapIndex)
    };

    deferredResolvePipeline->draw(cmd, deferredResolveDrawInfo);

    // TAA
    vk_helpers::transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    const VkImageLayout originLayout = frameNumber == 0 ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    vk_helpers::transitionImage(cmd, historyBuffer.image, originLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, taaResolveTarget.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

    TaaDrawInfo taaDrawInfo{
        renderExtent,
        taaBlend,
        bEnableTaa,
        taaDebug,
        sceneDataDescriptorBuffer,
        sceneDataDescriptorBuffer.getDescriptorBufferSize() * 0
    };
    taaPipeline->draw(cmd, taaDrawInfo);

    // Save current TAA Resolve to History
    vk_helpers::transitionImage(cmd, taaResolveTarget.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, historyBuffer.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::copyImageToImage(cmd, taaResolveTarget.image, historyBuffer.image, renderExtent, renderExtent);

    // Post-Process
    vk_helpers::transitionImage(cmd, postProcessOutputBuffer.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
    postProcessPipeline->draw(cmd, {renderExtent, postProcessFlags});
    vk_helpers::transitionImage(cmd, postProcessOutputBuffer.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

    // Final Swapchain Copy
    vk_helpers::transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::copyImageToImage(cmd, postProcessOutputBuffer.image, swapchainImages[swapchainImageIndex], renderExtent, swapchainExtent);

    // draw ImGui onto Swapchain Image
    vk_helpers::transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    imguiWrapper->drawImgui(cmd, swapchainImageViews[swapchainImageIndex], swapchainExtent);

    vk_helpers::transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkEndCommandBuffer(cmd));

    const auto renderEnd = std::chrono::system_clock::now();
    const float elapsedRenderMs = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(renderEnd - renderStart).count()) / 1000.0f;
    renderTime = renderTime * 0.99 + elapsedRenderMs * 0.01f;

    // Submission
    const VkCommandBufferSubmitInfo cmdinfo = vk_helpers::commandBufferSubmitInfo(cmd);
    const VkSemaphoreSubmitInfo waitInfo = vk_helpers::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, getCurrentFrame()._swapchainSemaphore);
    const VkSemaphoreSubmitInfo signalInfo = vk_helpers::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, getCurrentFrame()._renderSemaphore);
    const VkSubmitInfo2 submit = vk_helpers::submitInfo(&cmdinfo, &signalInfo, &waitInfo);

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

    const VkResult presentResult = vkQueuePresentKHR(context->graphicsQueue, &presentInfo);

    //increase the number of frames drawn
    frameNumber++;

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        bResizeRequested = true;
        fmt::print("Swapchain out of date or suboptimal, resize requested (At Present)\n");
    }

    const auto end = std::chrono::system_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    const float elapsedMs = static_cast<float>(elapsed.count()) / 1000.0f;

    frameTime = frameTime * 0.99f + elapsedMs * 0.01f;
}

void Engine::update(float deltaTime) const
{
    const Uint64 currentTime = SDL_GetTicks64();

    constexpr auto originalPosition = glm::vec3{0.0, 2.0f, 0.0f};
    const float timeSeconds = static_cast<float>(currentTime) / 1000.0f;
    constexpr float amplitude = 1.0f;
    constexpr float frequency = 0.25f;
    const float offset = std::sin(timeSeconds * frequency * 2.0f * glm::pi<float>()) * amplitude;
    const glm::vec3 vertical = originalPosition + glm::vec3{0.0f, offset, 0.0f};
    const glm::vec3 horizontal = originalPosition + glm::vec3{0.0f, 0.0f, offset} + glm::vec3(2.0f, 0.0f, 0.0f);

    cubeGameObject->setLocalPosition(vertical);
    cubeGameObject2->setLocalPosition(horizontal);

    player->update(deltaTime);

    if (player->isUsingDebugCamera() || bEnableShadowMapDebug) {
        constexpr int32_t numberOfCorners = 8;
        glm::vec4 corners[numberOfCorners];
        const FreeCamera* targetCamera = player->getFreeCamera();

        if (bShowPerspectiveBounds) {
            const CascadeSplit& split = cascadedShadowMap->getCascadeSplit(shadowMapDebug);
            render_utils::getPerspectiveFrustumCornersWorldSpace(split.nearPlane, split.farPlane, targetCamera->getFov(), targetCamera->getAspectRatio(), targetCamera->getPosition(), targetCamera->getForwardWS(),corners);
        }

        cameraDebugGameObjects[0]->setLocalPosition(targetCamera->getPosition());
        cameraDebugGameObjects[1]->setLocalPosition(corners[0]);
        cameraDebugGameObjects[2]->setLocalPosition(corners[1]);
        cameraDebugGameObjects[3]->setLocalPosition(corners[2]);
        cameraDebugGameObjects[4]->setLocalPosition(corners[3]);
        cameraDebugGameObjects[5]->setLocalPosition(corners[4]);
        cameraDebugGameObjects[6]->setLocalPosition(corners[5]);
        cameraDebugGameObjects[7]->setLocalPosition(corners[6]);
        cameraDebugGameObjects[8]->setLocalPosition(corners[7]);
        cameraDebugGameObjects[1]->setLocalScale(glm::pow(2, shadowMapDebug - 0.5));
        cameraDebugGameObjects[2]->setLocalScale(glm::pow(2, shadowMapDebug - 0.5));
        cameraDebugGameObjects[3]->setLocalScale(glm::pow(2, shadowMapDebug - 0.5));
        cameraDebugGameObjects[4]->setLocalScale(glm::pow(2, shadowMapDebug - 0.5));
        cameraDebugGameObjects[5]->setLocalScale(glm::pow(2, shadowMapDebug - 0.5));
        cameraDebugGameObjects[6]->setLocalScale(glm::pow(2, shadowMapDebug - 0.5));
        cameraDebugGameObjects[7]->setLocalScale(glm::pow(2, shadowMapDebug - 0.5));
        cameraDebugGameObjects[8]->setLocalScale(glm::pow(2, shadowMapDebug - 0.5));
    } else {
        constexpr glm::vec3 dormantPosition{0.0f, -50.0f, 0.0f};
        cameraDebugGameObjects[0]->setLocalPosition(dormantPosition);
        cameraDebugGameObjects[1]->setLocalPosition(dormantPosition);
        cameraDebugGameObjects[2]->setLocalPosition(dormantPosition);
        cameraDebugGameObjects[3]->setLocalPosition(dormantPosition);
        cameraDebugGameObjects[4]->setLocalPosition(dormantPosition);
        cameraDebugGameObjects[5]->setLocalPosition(dormantPosition);
        cameraDebugGameObjects[6]->setLocalPosition(dormantPosition);
        cameraDebugGameObjects[7]->setLocalPosition(dormantPosition);
        cameraDebugGameObjects[8]->setLocalPosition(dormantPosition);
    }
}

void Engine::cleanup()
{
    fmt::print("----------------------------------------\n");
    fmt::print("Cleaning up Will Engine V2\n");

    vkDeviceWaitIdle(context->device);

    delete environmentDescriptorLayouts;
    delete shadowMapDescriptorLayouts;
    delete sceneDescriptorLayouts;
    delete frustumCullDescriptorLayouts;
    delete renderObjectDescriptorLayout;

    delete frustumCullingPipeline;
    delete environmentPipeline;
    delete deferredMrtPipeline;
    delete deferredResolvePipeline;
    delete taaPipeline;
    delete postProcessPipeline;

    delete cascadedShadowMap;
    delete environmentMap;

    for (const auto gameObject : gameObjects) {
        delete gameObject;
    }
    gameObjects.clear();
    delete cubeGameObject;
    delete cubeGameObject2;
    delete sponzaObject;
    delete primitiveCubeGameObject;

    delete sponza;
    delete cube;
    delete primitives;

    delete player;

    // destroy all other resources
    //mainDeletionQueue.flush();
    resourceManager->destroyBuffer(sceneDataBuffer);
    sceneDataDescriptorBuffer.destroy(context->allocator);

    // Destroy these after destroying all render objects
    vkDestroyDescriptorSetLayout(context->device, emptyDescriptorSetLayout, nullptr);

    // ImGui
    delete imguiWrapper;

    // Main Rendering Command and Fence
    for (const auto& frame : frames) {
        vkDestroyCommandPool(context->device, frame._commandPool, nullptr);

        //destroy sync objects
        vkDestroyFence(context->device, frame._renderFence, nullptr);
        vkDestroySemaphore(context->device, frame._renderSemaphore, nullptr);
        vkDestroySemaphore(context->device, frame._swapchainSemaphore, nullptr);
    }


    // Draw Images
    resourceManager->destroyImage(drawImage);
    resourceManager->destroyImage(depthImage);

    resourceManager->destroyImage(normalRenderTarget);
    resourceManager->destroyImage(albedoRenderTarget);
    resourceManager->destroyImage(pbrRenderTarget);
    resourceManager->destroyImage(velocityRenderTarget);
    resourceManager->destroyImage(taaResolveTarget);
    resourceManager->destroyImage(historyBuffer);
    resourceManager->destroyImage(postProcessOutputBuffer);

    // Immediate Command and Fence
    delete immediate;
    delete resourceManager;

    delete physics;

    // Swapchain
    vkDestroySwapchainKHR(context->device, swapchain, nullptr);
    for (const auto swapchainImageView : swapchainImageViews) {
        vkDestroyImageView(context->device, swapchainImageView, nullptr);
    }

    delete context;

    SDL_DestroyWindow(window);
}

void Engine::initRenderer()
{
    DescriptorLayoutBuilder layoutBuilder;
    emptyDescriptorSetLayout = layoutBuilder.build(context->device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, nullptr,
                                                   VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    sceneDataBuffer = resourceManager->createBuffer(sizeof(SceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    sceneDataDescriptorBuffer = DescriptorBufferUniform(*context, sceneDescriptorLayouts->getSceneDataLayout(), 1);
    std::vector<DescriptorUniformData> sceneDataBufferData{1};
    sceneDataBufferData[0] = DescriptorUniformData{.uniformBuffer = sceneDataBuffer, .allocSize = sizeof(SceneData)};
    sceneDataDescriptorBuffer.setupData(context->device, sceneDataBufferData);

    frustumCullingPipeline = new FrustumCullingPipeline(*context);
    frustumCullingPipeline->init(
        {sceneDescriptorLayouts->getSceneDataLayout(), frustumCullDescriptorLayouts->getFrustumCullLayout()}
    );

    environmentPipeline = new EnvironmentPipeline(*context);
    environmentPipeline->init(
        {sceneDescriptorLayouts->getSceneDataLayout(), environmentDescriptorLayouts->getCubemapSamplerLayout(), drawImageFormat, depthImageFormat}
    );

    cascadedShadowMap = new CascadedShadowMap(*context, *resourceManager);

    cascadedShadowMap->init(
        {renderObjectDescriptorLayout->getAddressesLayout(), shadowMapDescriptorLayouts->getCascadedShadowMapSamplerLayout(), shadowMapDescriptorLayouts->getCascadedShadowMapUniformLayout(), player->getCamera()->getNearPlane(), player->getCamera()->getFarPlane()}
    );

    deferredMrtPipeline = new DeferredMrtPipeline(*context);
    deferredMrtPipeline->init(
        {sceneDescriptorLayouts->getSceneDataLayout(), renderObjectDescriptorLayout->getAddressesLayout(), renderObjectDescriptorLayout->getTexturesLayout()},
        {normalImageFormat, albedoImageFormat, pbrImageFormat, velocityImageFormat, depthImageFormat}
    );

    deferredResolvePipeline = new DeferredResolvePipeline(*context);
    deferredResolvePipeline->init(
        {sceneDescriptorLayouts->getSceneDataLayout(), environmentDescriptorLayouts->getEnvironmentMapLayout(), emptyDescriptorSetLayout}
    );
    deferredResolvePipeline->setupDescriptorBuffer(
        {
            normalRenderTarget.imageView, albedoRenderTarget.imageView, pbrRenderTarget.imageView, depthImage.imageView,
            velocityRenderTarget.imageView, drawImage.imageView, resourceManager->getDefaultSamplerNearest()
        }
    );

    taaPipeline = new TaaPipeline(*context);
    taaPipeline->init(sceneDescriptorLayouts->getSceneDataLayout());
    taaPipeline->setupDescriptorBuffer(
        {
            drawImage.imageView, historyBuffer.imageView, depthImage.imageView,
            velocityRenderTarget.imageView, taaResolveTarget.imageView, resourceManager->getDefaultSamplerLinear()
        }
    );

    postProcessPipeline = new PostProcessPipeline(*context);
    postProcessPipeline->init();
    postProcessPipeline->setupDescriptorBuffer(
        {taaResolveTarget.imageView, postProcessOutputBuffer.imageView, resourceManager->getDefaultSamplerNearest()}
    );
}

void Engine::initScene()
{
    player = new PlayerCharacter("Player Character");
    // There is limit of 10!
    environmentMap = new Environment(*context, *resourceManager, *immediate, *environmentDescriptorLayouts);
    const std::filesystem::path envMapSource = "assets/environments";
    //environmentMap->loadEnvironment("Meadow", (envMapSource / "meadow_4k.hdr").string().c_str(), 0);
    //environmentMap->loadEnvironment("Wasteland", (envMapSource / "wasteland_clouds_puresky_4k.hdr").string().c_str(), 2);
    environmentMap->loadEnvironment("Overcast Sky", (envMapSource / "kloofendal_overcast_puresky_4k.hdr").string().c_str(), 0);
    //environmentMap->loadEnvironment("Sunset Sky", (envMapSource / "belfast_sunset_puresky_4k.hdr").string().c_str(), 7);

    //testRenderObject = new RenderObject{this, "assets/models/BoxTextured/glTF/BoxTextured.gltf"};
    //testRenderObject = new RenderObject{this, "assets/models/structure_mat.glb"};
    //testRenderObject = new RenderObject{this, "assets/models/structure.glb"};
    //testRenderObject = new RenderObject{this, "assets/models/Suzanne/glTF/Suzanne.gltf"};

    RenderObjectLayouts renderObjectLayouts{};
    renderObjectLayouts.frustumCullLayout = frustumCullDescriptorLayouts->getFrustumCullLayout();
    renderObjectLayouts.addressesLayout = renderObjectDescriptorLayout->getAddressesLayout();
    renderObjectLayouts.texturesLayout = renderObjectDescriptorLayout->getTexturesLayout();
    sponza = new RenderObject{*context, *resourceManager, "assets/models/sponza2/Sponza.gltf", renderObjectLayouts};
    cube = new RenderObject{*context, *resourceManager, "assets/models/cube.gltf", renderObjectLayouts};
    primitives = new RenderObject{*context, *resourceManager, "assets/models/primitives/primitives.gltf", renderObjectLayouts};

    gameObjects.reserve(10);


    //primitives->generateGameObject(0) - This is a cube, and it is twice as big as a unit cube. For this reason, scale/physics shape will be identical
    gameObjects.emplace_back(primitives->generateGameObject(0));
    GameObject* floor = gameObjects.back();
    floor->setGlobalScale({20.0f, 1.0f, 20.0f});
    floor->translate({0.0f, -1.0f, 0.0f});
    const auto floorShape = new JPH::BoxShape(JPH::Vec3(20.0f, 1.0f, 20.0f));
    floor->setupRigidbody(floorShape);

    cameraDebugGameObjects.emplace_back(primitives->generateGameObject(2));
    GameObject* cameraRepresentation = cameraDebugGameObjects.back();
    cameraRepresentation->setGlobalScale(5.0f);

    cameraDebugGameObjects.emplace_back(primitives->generateGameObject(0));
    GameObject* cascade0Corner0 = cameraDebugGameObjects.back();
    cameraDebugGameObjects.emplace_back(primitives->generateGameObject(0));
    GameObject* cascade0Corner1 = cameraDebugGameObjects.back();
    cameraDebugGameObjects.emplace_back(primitives->generateGameObject(0));
    GameObject* cascade0Corner2 = cameraDebugGameObjects.back();
    cameraDebugGameObjects.emplace_back(primitives->generateGameObject(0));
    GameObject* cascade0Corner3 = cameraDebugGameObjects.back();
    cameraDebugGameObjects.emplace_back(primitives->generateGameObject(1));
    GameObject* cascade0Corner4 = cameraDebugGameObjects.back();
    cameraDebugGameObjects.emplace_back(primitives->generateGameObject(1));
    GameObject* cascade0Corner5 = cameraDebugGameObjects.back();
    cameraDebugGameObjects.emplace_back(primitives->generateGameObject(1));
    GameObject* cascade0Corner6 = cameraDebugGameObjects.back();
    cameraDebugGameObjects.emplace_back(primitives->generateGameObject(1));
    GameObject* cascade0Corner7 = cameraDebugGameObjects.back();
    cascade0Corner0->setGlobalScale(5.0f);
    cascade0Corner1->setGlobalScale(5.0f);
    cascade0Corner2->setGlobalScale(5.0f);
    cascade0Corner3->setGlobalScale(5.0f);
    cascade0Corner4->setGlobalScale(5.0f);
    cascade0Corner5->setGlobalScale(5.0f);
    cascade0Corner6->setGlobalScale(5.0f);
    cascade0Corner7->setGlobalScale(5.0f);

    // gameObjects.emplace_back(primitives->generateGameObject(0));
    // gameObjects[0]->setName("Cube 1");
    // GameObject* cube2 = primitives->generateGameObject(0);
    // GameObject* cube3 = primitives->generateGameObject(0);
    // GameObject* cube4 = primitives->generateGameObject(0);
    // GameObject* cube5 = primitives->generateGameObject(0);

    for (GameObject* gameObject : gameObjects) {
        scene.addGameObject(gameObject);
    }

    for (GameObject* gameObject : cameraDebugGameObjects) {
        scene.addGameObject(gameObject);
    }

    cubeGameObject = cube->generateGameObject(0);
    cubeGameObject->setName("Vertical Moving Cube");
    cubeGameObject2 = cube->generateGameObject(0);
    cubeGameObject2->setName("Horizontal Moving Cube");
    sponzaObject = sponza->generateGameObject();
    sponzaObject->translate({15, 0, 15});


    primitiveCubeGameObject = primitives->generateGameObject(0);
    primitives->attachToGameObject(player, 3);


    scene.addGameObject(sponzaObject);
    scene.addGameObject(cubeGameObject);
    scene.addGameObject(cubeGameObject2);
    scene.addGameObject(primitiveCubeGameObject);
    scene.addGameObject(player);


    primitiveCubeGameObject->translate({0.f, 3.0f, 0.f});
    primitiveCubeGameObject->setGlobalScale(0.5f);

    player->translate({3.0f, 2.0f, 0.0f});
    player->setGlobalScale(0.5f);

    cubeGameObject->setGlobalScale(0.05f);
    cubeGameObject2->setGlobalScale(0.05f);


    const auto movingBoxShape = new JPH::BoxShape(JPH::Vec3(0.05f, 0.05f, 0.05f));

    cubeGameObject->setupRigidbody(movingBoxShape, JPH::EMotionType::Dynamic, Layers::MOVING);
    cubeGameObject2->setupRigidbody(movingBoxShape, JPH::EMotionType::Dynamic, Layers::MOVING);
    primitiveCubeGameObject->setupRigidbody(physics->getUnitCubeShape(), JPH::EMotionType::Dynamic, Layers::MOVING);
    player->setupRigidbody(physics->getUnitSphereShape(), JPH::EMotionType::Dynamic, Layers::PLAYER);
}

void Engine::createSwapchain(const uint32_t width, const uint32_t height)
{
    vkb::SwapchainBuilder swapchainBuilder{context->physicalDevice, context->device, context->surface};

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

        VkImageViewCreateInfo rview_info = vk_helpers::imageviewCreateInfo(drawImage.imageFormat, drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
        VK_CHECK(vkCreateImageView(context->device, &rview_info, nullptr, &drawImage.imageView));
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

    // Render Targets
    {
        const auto generateRenderTarget = std::function([this](const VkFormat renderTargetFormat) {
            const VkExtent3D imageExtent = {renderExtent.width, renderExtent.height, 1};
            VkImageUsageFlags usageFlags{};
            usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
            usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            usageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
            VmaAllocationCreateInfo renderImageAllocationInfo = {};
            renderImageAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            renderImageAllocationInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            AllocatedImage renderTarget{};
            renderTarget.imageFormat = renderTargetFormat;
            renderTarget.imageExtent = imageExtent;
            const VkImageCreateInfo imageInfo = vk_helpers::imageCreateInfo(renderTarget.imageFormat, usageFlags, imageExtent);
            vmaCreateImage(context->allocator, &imageInfo, &renderImageAllocationInfo, &renderTarget.image, &renderTarget.allocation,
                           nullptr);

            const VkImageViewCreateInfo imageViewInfo = vk_helpers::imageviewCreateInfo(renderTarget.imageFormat, renderTarget.image,
                                                                                        VK_IMAGE_ASPECT_COLOR_BIT);
            VK_CHECK(vkCreateImageView(context->device, &imageViewInfo, nullptr, &renderTarget.imageView));

            return renderTarget;
        });

        normalRenderTarget = generateRenderTarget(normalImageFormat);
        albedoRenderTarget = generateRenderTarget(albedoImageFormat);
        pbrRenderTarget = generateRenderTarget(pbrImageFormat);
        velocityRenderTarget = generateRenderTarget(velocityImageFormat);
    }
    // TAA Resolve
    {
        taaResolveTarget.imageFormat = drawImageFormat;
        const VkExtent3D drawImageExtent = {renderExtent.width, renderExtent.height, 1};
        taaResolveTarget.imageExtent = drawImageExtent;
        VkImageUsageFlags taaResolveUsages{};
        taaResolveUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
        taaResolveUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        taaResolveUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;

        const VkImageCreateInfo taaResolveImageInfo = vk_helpers::imageCreateInfo(taaResolveTarget.imageFormat, taaResolveUsages, drawImageExtent);

        VmaAllocationCreateInfo taaResolveAllocationInfo = {};
        taaResolveAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        taaResolveAllocationInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vmaCreateImage(context->allocator, &taaResolveImageInfo, &taaResolveAllocationInfo, &taaResolveTarget.image, &taaResolveTarget.allocation, nullptr);

        const VkImageViewCreateInfo taaResolveImageViewInfo = vk_helpers::imageviewCreateInfo(taaResolveTarget.imageFormat, taaResolveTarget.image, VK_IMAGE_ASPECT_COLOR_BIT);
        VK_CHECK(vkCreateImageView(context->device, &taaResolveImageViewInfo, nullptr, &taaResolveTarget.imageView));
    }
    // Draw History
    {
        historyBuffer.imageFormat = drawImageFormat;
        const VkExtent3D drawImageExtent = {renderExtent.width, renderExtent.height, 1};
        historyBuffer.imageExtent = drawImageExtent;
        VkImageUsageFlags historyBufferUsages{};
        historyBufferUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
        historyBufferUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        const VkImageCreateInfo historyBufferImageInfo = vk_helpers::imageCreateInfo(historyBuffer.imageFormat, historyBufferUsages, drawImageExtent);

        VmaAllocationCreateInfo historyBufferAllocationInfo = {};
        historyBufferAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        historyBufferAllocationInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vmaCreateImage(context->allocator, &historyBufferImageInfo, &historyBufferAllocationInfo, &historyBuffer.image, &historyBuffer.allocation, nullptr);

        const VkImageViewCreateInfo historyBufferImageViewInfo = vk_helpers::imageviewCreateInfo(historyBuffer.imageFormat, historyBuffer.image, VK_IMAGE_ASPECT_COLOR_BIT);
        VK_CHECK(vkCreateImageView(context->device, &historyBufferImageViewInfo, nullptr, &historyBuffer.imageView));
    }
    // Post Process Result
    {
        postProcessOutputBuffer.imageFormat = drawImageFormat;
        const VkExtent3D drawImageExtent = {renderExtent.width, renderExtent.height, 1};
        postProcessOutputBuffer.imageExtent = drawImageExtent;
        VkImageUsageFlags usageFlags{};
        usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        const VkImageCreateInfo imageCreateInfo = vk_helpers::imageCreateInfo(postProcessOutputBuffer.imageFormat, usageFlags, drawImageExtent);

        VmaAllocationCreateInfo imageAllocationInfo = {};
        imageAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        imageAllocationInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vmaCreateImage(context->allocator, &imageCreateInfo, &imageAllocationInfo, &postProcessOutputBuffer.image, &postProcessOutputBuffer.allocation, nullptr);

        VkImageViewCreateInfo rview_info = vk_helpers::imageviewCreateInfo(postProcessOutputBuffer.imageFormat, postProcessOutputBuffer.image, VK_IMAGE_ASPECT_COLOR_BIT);
        VK_CHECK(vkCreateImageView(context->device, &rview_info, nullptr, &postProcessOutputBuffer.imageView));
    }
}
