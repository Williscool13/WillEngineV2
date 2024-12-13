//
// Created by William on 8/11/2024.
//


#include "engine.h"

#include <cmath>
#include <filesystem>

#include "input.h"
#include "../renderer/vk_pipelines.h"
#include "../util/time_utils.h"
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

#include "../util/file_utils.h"
#include "../util/halton.h"
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "src/physics/physics.h"
#include "src/renderer/immediate_submitter.h"
#include "src/renderer/resource_manager.h"
#include "src/renderer/environment/environment.h"

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

    // emp
    {
        DescriptorLayoutBuilder layoutBuilder;
        emptyDescriptorSetLayout = layoutBuilder.build(context->device,
                                                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
                                                       nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }

    initDearImgui();

    environmentDescriptorLayouts = new EnvironmentDescriptorLayouts(*context);
    sceneDescriptorLayouts = new SceneDescriptorLayouts(*context);
    frustumCullDescriptorLayouts = new FrustumCullingDescriptorLayouts(*context);
    renderObjectDescriptorLayout = new RenderObjectDescriptorLayout(*context);


    initScene();

    initDescriptors();

    frustumCullingPipeline = new FrustumCullingPipeline(*context);
    frustumCullingPipeline->init(
        {sceneDescriptorLayouts->getSceneDataLayout(), frustumCullDescriptorLayouts->getFrustumCullLayout()}
    );

    environmentPipeline = new EnvironmentPipeline(*context);
    environmentPipeline->init(
        {sceneDescriptorLayouts->getSceneDataLayout(), environmentDescriptorLayouts->getCubemapSamplerLayout(), drawImageFormat, depthImageFormat}
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
    taaPipeline->init();
    taaPipeline->setupDescriptorBuffer(
        {
            drawImage.imageView, historyBuffer.imageView, depthImage.imageView,
            velocityRenderTarget.imageView, taaResolveTarget.imageView, resourceManager->getDefaultSamplerNearest()
        }
    );

    postProcessPipeline = new PostProcessPipeline(*context);
    postProcessPipeline->init();
    postProcessPipeline->setupDescriptorBuffer(
        {taaResolveTarget.imageView, postProcessOutputBuffer.imageView, resourceManager->getDefaultSamplerNearest()}
    );;


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

            // imgui input handling
            ImGui_ImplSDL2_ProcessEvent(&e);

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

        // DearImGui Draw
        {
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            if (ImGui::Begin("Main")) {
                ImGui::Text("Frame Time: %.2f ms", frameTime);
                ImGui::Text("Draw Time: %.2f ms", drawTime);
                ImGui::Text("Render Time: %.2f ms", renderTime);
                ImGui::Text("Delta Time: %.2f ms", time.getDeltaTime() * 1000.0f);

                ImGui::Separator();
                ImGui::Text("TAA Properties:");
                ImGui::Checkbox("Enable TAA", &bEnableTaa);
                ImGui::Checkbox("Enable Jitter", &bEnableJitter);
                if (bEnableTaa) {
                    ImGui::SetNextItemWidth(100);
                    ImGui::InputFloat("Min Blend", &taaMinBlend);
                    ImGui::SetNextItemWidth(100);
                    ImGui::InputFloat("Max Blend", &taaMaxBlend);
                    ImGui::SetNextItemWidth(100);
                    ImGui::InputFloat("Velocity Weight", &taaVelocityWeight);
                    ImGui::Text("Taa Debug View");
                    ImGui::SetNextItemWidth(100);
                    const char* taaDebugLabels[] = {"None", "Confident", "Luma", "Depth", "Velocity", "Depth Delta"};
                    ImGui::Combo("TAA Debug View", &taaDebug, taaDebugLabels, IM_ARRAYSIZE(taaDebugLabels));
                }

                ImGui::Checkbox("Enable Post-Process", &bEnablePostProcess);


                ImGui::Separator();
                const char* debugLabels[] = {"None", "Velocity Buffer", "Depth Buffer"};
                ImGui::Text("Debug View");
                ImGui::SetNextItemWidth(100);
                ImGui::Combo("Debug View", &deferredDebug, debugLabels, IM_ARRAYSIZE(debugLabels));


                ImGui::Separator();
                if (ImGui::BeginChild("Spectate")) {
                    ImGui::Checkbox("Enabled Spectate Camera", &bSpectateCameraActive);
                    if (bSpectateCameraActive) {
                        if (ImGui::TreeNode("Camera Position")) {
                            ImGui::DragFloat3("Position", &spectateCameraPosition.x, 0.1f);
                            ImGui::TreePop();
                        }

                        // Modify spectateCameraLookAt
                        if (ImGui::TreeNode("Camera Look At")) {
                            ImGui::DragFloat3("Look At", &spectateCameraLookAt.x, 0.1f);
                            ImGui::TreePop();
                        }
                    }
                }
                ImGui::EndChild();
            }
            ImGui::End();

            if (ImGui::Begin("Camera")) {
                glm::vec3 viewDir = camera.getViewDirectionWS();
                glm::vec3 cameraRotation = camera.transform.getEulerAngles();
                cameraRotation = glm::degrees(cameraRotation);
                glm::vec3 cameraPosition = camera.transform.getPosition();
                ImGui::Text("View Direction: (%.2f, %.2f, %.2f)", viewDir.x, viewDir.y, viewDir.z);
                ImGui::Text("Rotation (%.2f, %.2f, %.2f)", cameraRotation.x, cameraRotation.y, cameraRotation.z);
                ImGui::Text("Position: (%.2f, %.2f, %.2f)", cameraPosition.x, cameraPosition.y, cameraPosition.z);

                if (ImGui::BeginChild("Input Details")) {
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
                    constexpr SDL_Keycode keyCodes[] = {SDLK_w, SDLK_a, SDLK_s, SDLK_d, SDLK_SPACE, SDLK_c, SDLK_LSHIFT};

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
                    ImGui::Text("Mouse Delta: (%.1f, %.1f)", input.getMouseXDelta(), input.getMouseYDelta());
                }
                ImGui::EndChild();
            }
            ImGui::End();

            if (ImGui::Begin("Save Render Targets")) {
                if (ImGui::Button("Save Draw Image")) {
                    if (file_utils::getOrCreateDirectory(file_utils::imagesSavePath)) {
                        std::filesystem::path path = file_utils::imagesSavePath / "drawImage.png";
                        vk_helpers::saveImageRGBA16SFLOAT(*resourceManager, *immediate, drawImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
                                                          path.string().c_str());
                    } else {
                        fmt::print(" Failed to find/create image save path directory");
                    }
                }

                if (ImGui::Button("Save Depth Image")) {
                    if (file_utils::getOrCreateDirectory(file_utils::imagesSavePath)) {
                        std::filesystem::path path = file_utils::imagesSavePath / "depthImage.png";
                        auto depthNormalize = [](const float depth) {
                            return logf(1.0f + depth * 15.0f) / logf(16.0f);
                        };
                        vk_helpers::saveImageR32F(*resourceManager, *immediate, depthImage, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT,
                                                  path.string().c_str(), depthNormalize);
                    } else {
                        fmt::print(" Failed to find/create image save path directory");
                    }
                }

                if (ImGui::Button("Save Normal Render Target")) {
                    if (file_utils::getOrCreateDirectory(file_utils::imagesSavePath)) {
                        std::filesystem::path path = file_utils::imagesSavePath / "normalRT.png";
                        auto unpackFunc = [](const uint32_t packedColor) {
                            glm::vec4 pixel = glm::unpackSnorm4x8(packedColor);
                            pixel.r = pixel.r * 0.5f + 0.5f;
                            pixel.g = pixel.g * 0.5f + 0.5f;
                            pixel.b = pixel.b * 0.5f + 0.5f;
                            pixel.a = 1.0f;
                            return pixel;
                        };

                        vk_helpers::savePacked32Bit(*resourceManager, *immediate, normalRenderTarget, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                    VK_IMAGE_ASPECT_COLOR_BIT, path.string().c_str(), unpackFunc);
                    } else {
                        fmt::print(" Failed to save normal render target");
                    }
                }

                if (ImGui::Button("Save Albedo Render Target")) {
                    if (file_utils::getOrCreateDirectory(file_utils::imagesSavePath)) {
                        std::filesystem::path path = file_utils::imagesSavePath / "albedoRT.png";
                        auto unpackFunc = [](const uint32_t packedColor) {
                            glm::vec4 pixel = glm::unpackUnorm4x8(packedColor);
                            pixel.a = 1.0f;
                            return pixel;
                        };

                        vk_helpers::savePacked32Bit(*resourceManager, *immediate, albedoRenderTarget, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                    VK_IMAGE_ASPECT_COLOR_BIT, path.string().c_str(), unpackFunc);
                    } else {
                        fmt::print(" Failed to save albedo render target");
                    }
                }

                if (ImGui::Button("Save PBR Render Target")) {
                    if (file_utils::getOrCreateDirectory(file_utils::imagesSavePath)) {
                        std::filesystem::path path = file_utils::imagesSavePath / "pbrRT.png";
                        auto unpackFunc = [](const uint32_t packedColor) {
                            glm::vec4 pixel = glm::unpackUnorm4x8(packedColor);
                            pixel.a = 1.0f;
                            return pixel;
                        };
                        vk_helpers::savePacked32Bit(*resourceManager, *immediate, pbrRenderTarget, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                    VK_IMAGE_ASPECT_COLOR_BIT, path.string().c_str(), unpackFunc);
                    } else {
                        fmt::print(" Failed to save pbr render target");
                    }
                }

                if (ImGui::Button("Save Post Process Resolve Target")) {
                    if (file_utils::getOrCreateDirectory(file_utils::imagesSavePath)) {
                        if (file_utils::getOrCreateDirectory(file_utils::imagesSavePath)) {
                            std::filesystem::path path = file_utils::imagesSavePath / "postProcesResolve.png";
                            vk_helpers::saveImageRGBA16SFLOAT(*resourceManager, *immediate, postProcessOutputBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
                                                              path.string().c_str());
                        } else {
                            fmt::print(" Failed to find/create image save path directory");
                        }
                    }
                }
            }
            ImGui::End();

            if (ImGui::Begin("Environment Map")) {
                const std::unordered_map<int32_t, const char*>& activeEnvironmentMapNames = environmentMap->getActiveEnvironmentMapNames();

                std::vector<std::pair<int32_t, std::string> > indexNamePairs;
                for (std::pair<const int, const char*> kvp : activeEnvironmentMapNames) {
                    indexNamePairs.emplace_back(kvp.first, kvp.second);
                }
                std::sort(indexNamePairs.begin(), indexNamePairs.end());
                auto it = std::ranges::find_if(indexNamePairs, [this](const auto& pair) {
                    return pair.first == environmentMapindex;
                });
                int currentIndex = (it != indexNamePairs.end()) ? static_cast<int>(std::distance(indexNamePairs.begin(), it)) : 0;
                struct ComboData
                {
                    const std::vector<std::pair<int32_t, std::string> >* pairs;
                };

                // ReSharper disable once CppParameterMayBeConst
                // ReSharper disable once CppParameterMayBeConstPtrOrRef
                auto getLabel = [](void* data, int idx, const char** out_text) -> bool {
                    static std::string label;
                    const auto& pairs = *static_cast<const ComboData*>(data)->pairs;
                    label = pairs[idx].second;
                    *out_text = label.c_str();
                    return true;
                };

                ComboData data{&indexNamePairs};
                if (ImGui::Combo("Select Environment Map", &currentIndex, getLabel, &data, static_cast<int>(indexNamePairs.size()))) {
                    environmentMapindex = indexNamePairs[currentIndex].first;
                }

                // Show both name and index in the status text
                ImGui::Text("Currently selected: %s (ID: %u)", indexNamePairs[currentIndex].second.c_str(), environmentMapindex);
            }
            ImGui::End();

            scene.imguiSceneGraph();

            ImGui::Render();
        }

        if (Input::Get().isKeyPressed(SDLK_p)) {
            GameObject* root = scene.DEBUG_getSceneRoot();

            root->transform.translate(glm::vec3(1.0f, 0.0f, 0.0f));
            root->dirty();
        }


        draw();
    }
}

void Engine::updateSceneData() const
{
    const bool bIsFrameZero = frameNumber == 0;

    const glm::vec2 currentJitter = HaltonSequence::getJitterHardcoded(frameNumber, {renderExtent.width, renderExtent.height});
    const glm::vec2 prevJitter = HaltonSequence::getJitterHardcoded(frameNumber > 0 ? frameNumber - 1 : 0, {renderExtent.width, renderExtent.height});

    // Update scene data
    {
        const auto pSceneData = static_cast<SceneData*>(sceneDataBuffer.info.pMappedData);

        pSceneData->prevView = bIsFrameZero ? camera.getViewMatrix() : pSceneData->view;
        pSceneData->prevProj = bIsFrameZero ? camera.getProjMatrix() : pSceneData->proj;
        pSceneData->prevViewProj = bIsFrameZero ? camera.getViewProjMatrix() : pSceneData->viewProj;
        pSceneData->jitter = bEnableJitter && bEnableTaa ? glm::vec4(currentJitter.x, currentJitter.y, prevJitter.x, prevJitter.y) : glm::vec4(0.0f);

        pSceneData->view = camera.getViewMatrix();
        pSceneData->proj = camera.getProjMatrix();
        pSceneData->viewProj = pSceneData->proj * pSceneData->view;
        pSceneData->invView = glm::inverse(pSceneData->view);
        pSceneData->invProj = glm::inverse(pSceneData->proj);
        pSceneData->invViewProj = glm::inverse(pSceneData->viewProj);
        pSceneData->cameraWorldPos = camera.getPosition();
        const glm::mat4 cameraLook = glm::lookAt(glm::vec3(0), camera.getViewDirectionWS(), glm::vec3(0, 1, 0));
        pSceneData->viewProjCameraLookDirection = pSceneData->proj * cameraLook;

        pSceneData->frameNumber = getCurrentFrameOverlap();
        pSceneData->renderTargetSize = {renderExtent.width, renderExtent.height};
        pSceneData->deltaTime = TimeUtils::Get().getDeltaTime();
    }
    // Update spectate scene data
    {
        const auto pSpectateSceneData = static_cast<SceneData*>(spectateSceneDataBuffer.info.pMappedData);
        const auto spectateView = glm::lookAt(spectateCameraPosition, spectateCameraLookAt, glm::vec3(0.f, 1.0f, 0.f));

        pSpectateSceneData->prevView = bIsFrameZero ? spectateView : pSpectateSceneData->view;
        pSpectateSceneData->prevProj = bIsFrameZero ? camera.getProjMatrix() : pSpectateSceneData->proj;
        pSpectateSceneData->prevViewProj = bIsFrameZero ? camera.getProjMatrix() * spectateView : pSpectateSceneData->viewProj;
        pSpectateSceneData->jitter = bEnableJitter && bEnableTaa ? glm::vec4(currentJitter.x, currentJitter.y, prevJitter.x, prevJitter.y) : glm::vec4(0.0f);

        pSpectateSceneData->view = spectateView;
        pSpectateSceneData->proj = camera.getProjMatrix();
        pSpectateSceneData->viewProj = pSpectateSceneData->proj * pSpectateSceneData->view;
        pSpectateSceneData->invView = glm::inverse(pSpectateSceneData->view);
        pSpectateSceneData->invProj = glm::inverse(pSpectateSceneData->proj);
        pSpectateSceneData->invViewProj = glm::inverse(pSpectateSceneData->viewProj);
        pSpectateSceneData->cameraWorldPos = glm::vec4(spectateCameraPosition, 0.f);
        pSpectateSceneData->viewProjCameraLookDirection = spectateView;
    }
}

void Engine::updateSceneObjects() const
{
    const Uint64 currentTime = SDL_GetTicks64();
    constexpr auto originalPosition = glm::vec3{0.0, 2.0f, 0.0f};
    const float timeSeconds = static_cast<float>(currentTime) / 1000.0f;
    constexpr float amplitude = 1.0f;
    constexpr float frequency = 0.25f;
    const float offset = std::sin(timeSeconds * frequency * 2.0f * glm::pi<float>()) * amplitude;
    const glm::vec3 vertical = originalPosition + glm::vec3{0.0f, offset, 0.0f};
    const glm::vec3 horizontal = originalPosition + glm::vec3{0.0f, 0.0f, offset} + glm::vec3(2.0f, 0.0f, 0.0f);

    cubeGameObject->transform.setPosition(vertical);
    cubeGameObject->dirty();
    cubeGameObject2->transform.setPosition(horizontal);
    cubeGameObject2->dirty();


    scene.updateSceneModelMatrices();
}

void Engine::draw()
{
    const auto start = std::chrono::system_clock::now();

    camera.update();
    updateSceneData();
    updateSceneObjects();
    physics->update(TimeUtils::Get().getDeltaTime());
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



    const auto renderStart = std::chrono::system_clock::now();

    const auto cmd = getCurrentFrame()._mainCommandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    const VkCommandBufferBeginInfo cmdBeginInfo = vk_helpers::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    const std::vector renderObjects{sponza, cube, primitives};
    frustumCullingPipeline->draw(cmd, {renderObjects, sceneDataDescriptorBuffer});

    vk_helpers::transitionImage(cmd, depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
    vk_helpers::transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

    const EnvironmentDrawInfo environmentDrawInfo = {
        renderExtent, drawImage.imageView, depthImage.imageView, sceneDataDescriptorBuffer, environmentMap->getCubemapDescriptorBuffer(), environmentMap->getEnvironmentMapOffset(environmentMapindex)
    };
    environmentPipeline->draw(cmd, environmentDrawInfo);

    vk_helpers::transitionImage(cmd, normalRenderTarget.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, albedoRenderTarget.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, pbrRenderTarget.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, velocityRenderTarget.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

    DeferredMrtDrawInfo deferredMrtDrawInfo{sceneDataDescriptorBuffer};
    deferredMrtDrawInfo.renderObjects = renderObjects;
    deferredMrtDrawInfo.normalTarget = normalRenderTarget.imageView;
    deferredMrtDrawInfo.albedoTarget = albedoRenderTarget.imageView;
    deferredMrtDrawInfo.pbrTarget = pbrRenderTarget.imageView;
    deferredMrtDrawInfo.velocityTarget = velocityRenderTarget.imageView;
    deferredMrtDrawInfo.depthTarget = depthImage.imageView;
    deferredMrtDrawInfo.renderExtent = renderExtent;
    deferredMrtDrawInfo.viewportRenderExtent = {static_cast<float>(renderExtent.width), static_cast<float>(renderExtent.height)};

    deferredMrtPipeline->draw(cmd, deferredMrtDrawInfo);

    vk_helpers::transitionImage(cmd, normalRenderTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, albedoRenderTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, pbrRenderTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, velocityRenderTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, depthImage.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
    vk_helpers::transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

    DeferredResolveDrawInfo deferredResolveDrawInfo{sceneDataDescriptorBuffer};
    deferredResolveDrawInfo.renderExtent = renderExtent;
    deferredResolveDrawInfo.debugMode = taaDebug;
    deferredResolveDrawInfo.environment = environmentMap;
    deferredResolveDrawInfo.environmentMapIndex = environmentMapindex;

    deferredResolvePipeline->draw(cmd, deferredResolveDrawInfo);
    if (bSpectateCameraActive) { DEBUG_drawSpectate(cmd, renderObjects); }

    // TAA
    vk_helpers::transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    const VkImageLayout originLayout = frameNumber == 0 ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    vk_helpers::transitionImage(cmd, historyBuffer.image, originLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, taaResolveTarget.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

    taaPipeline->draw(cmd, {renderExtent, taaMinBlend, taaMaxBlend, taaVelocityWeight, bEnableTaa, taaDebug, camera});

    // Save current TAA Resolve to History
    vk_helpers::transitionImage(cmd, taaResolveTarget.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, historyBuffer.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::copyImageToImage(cmd, taaResolveTarget.image, historyBuffer.image, renderExtent, renderExtent);

    // Post-Process
    vk_helpers::transitionImage(cmd, postProcessOutputBuffer.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
    postProcessPipeline->draw(cmd, {renderExtent, bEnablePostProcess});
    vk_helpers::transitionImage(cmd, postProcessOutputBuffer.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

    // Final Swapchain Copy
    vk_helpers::transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::copyImageToImage(cmd, postProcessOutputBuffer.image, swapchainImages[swapchainImageIndex], renderExtent, swapchainExtent);

    // draw ImGui onto Swapchain Image
    vk_helpers::transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    drawImgui(cmd, swapchainImageViews[swapchainImageIndex]);

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
    drawTime = drawTime * 0.99f + elapsedMs * 0.01f;
}

void Engine::DEBUG_drawSpectate(VkCommandBuffer cmd, const std::vector<RenderObject*>& renderObjects) const
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Debug Draw Spectate Pass";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    // layout transition #1
    {
        vk_helpers::transitionImage(cmd, normalRenderTarget.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
        vk_helpers::transitionImage(cmd, albedoRenderTarget.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
        vk_helpers::transitionImage(cmd, pbrRenderTarget.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
        vk_helpers::transitionImage(cmd, velocityRenderTarget.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
        vk_helpers::transitionImage(cmd, depthImage.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
        vk_helpers::transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    // mrt
    {
        DeferredMrtDrawInfo deferredDrawInfo{spectateSceneDataDescriptorBuffer};
        deferredDrawInfo.renderObjects = renderObjects;
        deferredDrawInfo.normalTarget = normalRenderTarget.imageView;
        deferredDrawInfo.albedoTarget = albedoRenderTarget.imageView;
        deferredDrawInfo.pbrTarget = pbrRenderTarget.imageView;
        deferredDrawInfo.velocityTarget = velocityRenderTarget.imageView;
        deferredDrawInfo.depthTarget = depthImage.imageView;
        deferredDrawInfo.renderExtent = renderExtent;
        deferredDrawInfo.viewportRenderExtent = {static_cast<float>(renderExtent.width) / 3.0f, static_cast<float>(renderExtent.height) / 3.0f};

        deferredMrtPipeline->draw(cmd, deferredDrawInfo);
    }

    // layout transition #2
    {
        vk_helpers::transitionImage(cmd, normalRenderTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
        vk_helpers::transitionImage(cmd, albedoRenderTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
        vk_helpers::transitionImage(cmd, pbrRenderTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
        vk_helpers::transitionImage(cmd, velocityRenderTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
        vk_helpers::transitionImage(cmd, depthImage.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
        vk_helpers::transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    // resolve
    {
        DeferredResolveDrawInfo deferredResolveDrawInfo{sceneDataDescriptorBuffer};
        deferredResolveDrawInfo.renderExtent = renderExtent;
        deferredResolveDrawInfo.debugMode = taaDebug;
        deferredResolveDrawInfo.environment = environmentMap;
        deferredResolveDrawInfo.environmentMapIndex = environmentMapindex;

        deferredResolvePipeline->draw(cmd, deferredResolveDrawInfo);
    }

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

// ReSharper disable twice CppParameterMayBeConst
void Engine::drawImgui(VkCommandBuffer cmd, VkImageView targetImageView) const
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "DearImgui Draw Pass";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    const VkRenderingAttachmentInfo colorAttachment = vk_helpers::attachmentInfo(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const VkRenderingInfo renderInfo = vk_helpers::renderingInfo(swapchainExtent, &colorAttachment, nullptr);
    vkCmdBeginRendering(cmd, &renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRendering(cmd);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void Engine::cleanup()
{
    fmt::print("----------------------------------------\n");
    fmt::print("Cleaning up Will Engine V2\n");

    vkDeviceWaitIdle(context->device);

    delete environmentDescriptorLayouts;
    delete sceneDescriptorLayouts;
    delete frustumCullDescriptorLayouts;
    delete renderObjectDescriptorLayout;

    delete frustumCullingPipeline;
    delete environmentPipeline;
    delete deferredMrtPipeline;
    delete deferredResolvePipeline;
    delete taaPipeline;
    delete postProcessPipeline;

    delete environmentMap;
    delete cubeGameObject;
    delete cubeGameObject2;
    delete sponzaObject;
    delete primitiveCubeGameObject;

    delete sponza;
    delete cube;
    delete primitives;
    // destroy all other resources
    //mainDeletionQueue.flush();
    resourceManager->destroyBuffer(sceneDataBuffer);
    sceneDataDescriptorBuffer.destroy(context->device, context->allocator);
    resourceManager->destroyBuffer(spectateSceneDataBuffer);
    spectateSceneDataDescriptorBuffer.destroy(context->device, context->allocator);

    // Destroy these after destroying all render objects
    vkDestroyDescriptorSetLayout(context->device, emptyDescriptorSetLayout, nullptr);

    // ImGui
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(context->device, imguiPool, nullptr);

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

void Engine::initDearImgui()
{
    // DearImGui implementation, basically copied directly from the Vulkan/SDl2 from DearImGui samples.
    // Because this project uses VOLK, additionally need to load functions.
    // DYNAMIC RENDERING (NOT RENDER PASS)
    constexpr VkDescriptorPoolSize pool_sizes[] =
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
    VK_CHECK(vkCreateDescriptorPool(context->device, &pool_info, nullptr, &imguiPool));

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls

    ImGui::StyleColorsLight();

    VkInstance instance = context->instance;
    // Need to LoadFunction when using VOLK/using VK_NO_PROTOTYPES
    ImGui_ImplVulkan_LoadFunctions([](const char* function_name, void* vulkan_instance) {
        return vkGetInstanceProcAddr(*(static_cast<VkInstance*>(vulkan_instance)), function_name);
    }, &instance);

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForVulkan(window);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = context->instance;
    init_info.PhysicalDevice = context->physicalDevice;
    init_info.Device = context->device;
    init_info.QueueFamily = context->graphicsQueueFamily;
    init_info.Queue = context->graphicsQueue;
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

void Engine::initDescriptors()
{
    sceneDataBuffer = resourceManager->createBuffer(sizeof(SceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    sceneDataDescriptorBuffer = DescriptorBufferUniform(context->instance, context->device, context->physicalDevice, context->allocator, sceneDescriptorLayouts->getSceneDataLayout(), 1);
    std::vector<DescriptorUniformData> sceneDataBuffers{1};
    sceneDataBuffers[0] = DescriptorUniformData{.uniformBuffer = sceneDataBuffer, .allocSize = sizeof(SceneData)};
    sceneDataDescriptorBuffer.setupData(context->device, sceneDataBuffers);

    spectateSceneDataBuffer = resourceManager->createBuffer(sizeof(SceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    spectateSceneDataDescriptorBuffer = DescriptorBufferUniform(context->instance, context->device, context->physicalDevice, context->allocator, sceneDescriptorLayouts->getSceneDataLayout(), 1);
    sceneDataBuffers[0] = DescriptorUniformData{.uniformBuffer = spectateSceneDataBuffer, .allocSize = sizeof(SceneData)};
    spectateSceneDataDescriptorBuffer.setupData(context->device, sceneDataBuffers);
}

void Engine::initScene()
{
    // There is limit of 10!
    environmentMap = new Environment(*context, *resourceManager, *immediate, *environmentDescriptorLayouts);
    const std::filesystem::path envMapSource = "assets/environments";
    environmentMap->loadEnvironment("Meadow", (envMapSource / "meadow_4k.hdr").string().c_str(), 0);
    //environment->loadCubemap((envMapSource / "wasteland_clouds_4k.hdr").string().c_str(), 1);
    environmentMap->loadEnvironment("Wasteland", (envMapSource / "wasteland_clouds_puresky_4k.hdr").string().c_str(), 2);
    //environment->loadCubemap((envMapSource / "kloppenheim_06_puresky_4k.hdr").string().c_str(), 3);
    environmentMap->loadEnvironment("Overcast Sky", (envMapSource / "kloofendal_overcast_puresky_4k.hdr").string().c_str(), 4);
    //environment->loadCubemap((envMapSource / "mud_road_puresky_4k.hdr").string().c_str(), 5);
    //environment->loadCubemap((envMapSource / "sunflowers_puresky_4k.hdr").string().c_str(), 6);
    environmentMap->loadEnvironment("Sunset Sky", (envMapSource / "belfast_sunset_puresky_4k.hdr").string().c_str(), 7);

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

    cubeGameObject = cube->generateGameObject();
    cubeGameObject2 = cube->generateGameObject();
    sponzaObject = sponza->generateGameObject();
    primitiveCubeGameObject = primitives->generateGameObject(0);

    scene.addGameObject(sponzaObject);

    scene.addGameObject(cubeGameObject);
    scene.addGameObject(cubeGameObject2);

    scene.addGameObject(primitiveCubeGameObject);

    primitiveCubeGameObject->transform.translate({0.f, 3.0f, 0.f});
    primitiveCubeGameObject->dirty();

    sponzaObject->transform.setScale(1.f);
    sponzaObject->dirty();

    cubeGameObject->transform.setScale(0.05f);
    cubeGameObject->dirty();
    cubeGameObject2->transform.setScale(0.05f);
    cubeGameObject2->dirty();

    JPH::BoxShape boxShape = {JPH::Vec3(0.5f, 0.5f, 0.5f)};
    physics->addRigidBody(primitiveCubeGameObject, physics->getUnitCubeShape());
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
