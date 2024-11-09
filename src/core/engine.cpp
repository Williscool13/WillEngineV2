//
// Created by William on 8/11/2024.
//


#include "engine.h"

#include <filesystem>

#include "input.h"
#include "../renderer/vk_pipelines.h"
#include "../util/time_utils.h"
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

#include "render_object.h"
#include "../util/file_utils.h"
#include "../util/halton.h"

#ifdef NDEBUG
#define USE_VALIDATION_LAYERS false
#else
#define USE_VALIDATION_LAYERS true
#endif

VkSampler Engine::defaultSamplerLinear{VK_NULL_HANDLE};
VkSampler Engine::defaultSamplerNearest{VK_NULL_HANDLE};
AllocatedImage Engine::whiteImage{};
AllocatedImage Engine::errorCheckerboardImage{};
VkDescriptorSetLayout Engine::emptyDescriptorSetLayout;

void Engine::init()
{
    fmt::print("----------------------------------------\n");
    fmt::print("Initializing Will Engine V2\n");
    const auto start = std::chrono::system_clock::now(); {
        // We initialize SDL and create a window with it.
        SDL_Init(SDL_INIT_VIDEO);
        constexpr auto window_flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE);
        windowExtent = {1920, 1080};
        //constexpr auto window_flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

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

    initDescriptors();

    initPipelines();


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
                ImGui::Text("Delta Time: %.2f ms", time.getDeltaTime() * 1000.0f);

                ImGui::Separator();
                ImGui::Text("TAA Properties:");
                ImGui::Checkbox("Enable TAA", &bEnableTaa);
                if (bEnableTaa) {
                    ImGui::SetNextItemWidth(100);
                    ImGui::InputFloat("Min Blend", &taaMinBlend);
                    ImGui::SetNextItemWidth(100);
                    ImGui::InputFloat("Max Blend", &taaMaxBlend);
                    ImGui::SetNextItemWidth(100);
                    ImGui::InputFloat("Velocity Rejection Weight", &taaVelocityRejectionWeight);
                    ImGui::SetNextItemWidth(100);
                    ImGui::InputFloat("Depth Rejection Weight", &taaDepthRejectionWeight);
                }


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
                glm::vec3 cameraPosition = camera.transform.getTranslation();
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
                        vk_helpers::saveImageRGBA16SFLOAT(this, drawImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
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
                        vk_helpers::saveImageR32F(this, depthImage, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT,
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

                        vk_helpers::savePacked32Bit(this, normalRenderTarget, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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

                        vk_helpers::savePacked32Bit(this, albedoRenderTarget, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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
                        vk_helpers::savePacked32Bit(this, pbrRenderTarget, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                    VK_IMAGE_ASPECT_COLOR_BIT, path.string().c_str(), unpackFunc);
                    } else {
                        fmt::print(" Failed to save pbr render target");
                    }
                }
            }
            ImGui::End();

            if (ImGui::Begin("Environment Map")) {
                const std::unordered_map<int32_t, const char*>& activeEnvironmentMapNames = environment->getActiveEnvironmentMapNames();

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
    const glm::vec2 currentJitter = HaltonSequence::getJitter(frameNumber, {renderExtent.width, renderExtent.height});
    const glm::vec2 prevJitter = HaltonSequence::getJitter(frameNumber > 0 ? frameNumber - 1 : 0, {renderExtent.width, renderExtent.height});

    // Update scene data
    {
        const auto pSceneData = static_cast<SceneData*>(sceneDataBuffer.info.pMappedData);

        pSceneData->prevView = bIsFrameZero ? camera.getViewMatrix() : pSceneData->view;
        pSceneData->prevProj = bIsFrameZero ? camera.getProjMatrix() : pSceneData->proj;
        pSceneData->prevViewProj = bIsFrameZero ? camera.getViewProjMatrix() : pSceneData->viewProj;
        pSceneData->jitter = bEnableTaa ? glm::vec4(currentJitter.x, currentJitter.y, prevJitter.x, prevJitter.y) : glm::vec4(0.0f);

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
        pSpectateSceneData->jitter = glm::vec4(currentJitter.x, currentJitter.y, prevJitter.x, prevJitter.y);

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
    testGameObject1->recursiveUpdateModelMatrix(getCurrentFrameOverlap());
    testGameObject2->recursiveUpdateModelMatrix(getCurrentFrameOverlap());
    testGameObject3->recursiveUpdateModelMatrix(getCurrentFrameOverlap());
    testGameObject4->recursiveUpdateModelMatrix(getCurrentFrameOverlap());
    testGameObject5->recursiveUpdateModelMatrix(getCurrentFrameOverlap());
}

void Engine::draw()
{
    camera.update();

    const auto start = std::chrono::system_clock::now();

    // GPU -> CPU sync (fence)
    VK_CHECK(vkWaitForFences(device, 1, &getCurrentFrame()._renderFence, true, 1000000000));
    VK_CHECK(vkResetFences(device, 1, &getCurrentFrame()._renderFence));

    // GPU -> GPU sync (semaphore)
    uint32_t swapchainImageIndex;
    const VkResult e = vkAcquireNextImageKHR(device, swapchain, 1000000000, getCurrentFrame()._swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR) {
        bResizeRequested = true;
        fmt::print("Swapchain out of date or suboptimal, resize requested (At Acquire)\n");
        return;
    }

    updateSceneData();
    updateSceneObjects();

    // Start Command Buffer Recording
    const auto cmd = getCurrentFrame()._mainCommandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    const VkCommandBufferBeginInfo cmdBeginInfo = vk_helpers::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT); // only submit once
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    frustumCull(cmd);

    vk_helpers::transitionImage(cmd, depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
    vk_helpers::transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    drawEnvironment(cmd);

    vk_helpers::transitionImage(cmd, normalRenderTarget.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, albedoRenderTarget.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, pbrRenderTarget.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, velocityRenderTarget.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    drawDeferredMrt(cmd);

    vk_helpers::transitionImage(cmd, normalRenderTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, albedoRenderTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, pbrRenderTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, velocityRenderTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, depthImage.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
    vk_helpers::transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

    drawDeferredResolve(cmd);

    if (bEnableTaa && deferredDebug == 0) {
        vk_helpers::transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
        if (frameNumber == 0) {
            vk_helpers::transitionImage(cmd, historyBuffer.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
            vk_helpers::transitionImage(cmd, depthHistoryBuffer.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

        } else {
            vk_helpers::transitionImage(cmd, historyBuffer.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
            vk_helpers::transitionImage(cmd, depthHistoryBuffer.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
        }
        vk_helpers::transitionImage(cmd, taaResolveBuffer.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

        drawTaa(cmd);

        vk_helpers::transitionImage(cmd, taaResolveBuffer.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
        vk_helpers::transitionImage(cmd, historyBuffer.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
        vk_helpers::copyImageToImage(cmd, taaResolveBuffer.image, historyBuffer.image, renderExtent, renderExtent);

        vk_helpers::transitionImage(cmd, depthImage.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
        vk_helpers::transitionImage(cmd, depthHistoryBuffer.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
        vk_helpers::copyDepthToDepth(cmd, depthImage.image, depthHistoryBuffer.image, renderExtent, renderExtent);


        vk_helpers::transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
        vk_helpers::copyImageToImage(cmd, taaResolveBuffer.image, swapchainImages[swapchainImageIndex], renderExtent, swapchainExtent);
    } else {
        if (bSpectateCameraActive) {
            DEBUG_drawSpectate(cmd);
        }

        // Copy Draw Image into Swapchain Image
        vk_helpers::transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
        vk_helpers::transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
        vk_helpers::copyImageToImage(cmd, drawImage.image, swapchainImages[swapchainImageIndex], renderExtent, swapchainExtent);
    }

    // draw ImGui into Swapchain Image
    vk_helpers::transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    drawImgui(cmd, swapchainImageViews[swapchainImageIndex]);

    vk_helpers::transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_ASPECT_COLOR_BIT);

    // End Command Buffer Recording
    VK_CHECK(vkEndCommandBuffer(cmd));

    // Submission
    const VkCommandBufferSubmitInfo cmdinfo = vk_helpers::commandBufferSubmitInfo(cmd);
    const VkSemaphoreSubmitInfo waitInfo = vk_helpers::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, getCurrentFrame()._swapchainSemaphore);
    const VkSemaphoreSubmitInfo signalInfo = vk_helpers::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, getCurrentFrame()._renderSemaphore);
    const VkSubmitInfo2 submit = vk_helpers::submitInfo(&cmdinfo, &signalInfo, &waitInfo);

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

    const VkResult presentResult = vkQueuePresentKHR(graphicsQueue, &presentInfo);

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

// ReSharper disable once CppParameterMayBeConst
void Engine::frustumCull(VkCommandBuffer cmd) const
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Frustum Culling";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    // GPU Frustum Culling
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, frustumCullingPipeline);

    VkDescriptorBufferBindingInfoEXT frustumCullingBindingInfo[2]{};
    frustumCullingBindingInfo[0] = sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo();
    frustumCullingBindingInfo[1] = testRenderObject->getComputeAddressesDescriptorBuffer().getDescriptorBufferBindingInfo();
    vkCmdBindDescriptorBuffersEXT(cmd, 2, frustumCullingBindingInfo);

    constexpr VkDeviceSize offset{0};
    constexpr uint32_t sceneDataIndex{0};
    constexpr uint32_t addressesIndex{1};

    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, frustumCullingPipelineLayout, 0, 1, &sceneDataIndex, &offset);
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, frustumCullingPipelineLayout, 1, 1, &addressesIndex, &offset);

    vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(static_cast<float>(testRenderObject->getDrawIndirectCommandCount()) / 64.0f)), 1, 1);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

// ReSharper disable once CppParameterMayBeConst
void Engine::drawEnvironment(VkCommandBuffer cmd) const
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Environment Map";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    constexpr VkClearValue depthClearValue = {0.0f, 0.0f};
    const VkRenderingAttachmentInfo colorAttachment = vk_helpers::attachmentInfo(drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const VkRenderingAttachmentInfo depthAttachment = vk_helpers::attachmentInfo(depthImage.imageView, &depthClearValue, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    const VkRenderingInfo renderInfo = vk_helpers::renderingInfo(renderExtent, &colorAttachment, &depthAttachment);
    vkCmdBeginRendering(cmd, &renderInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, environmentPipeline);

    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<float>(renderExtent.width);
    viewport.height = static_cast<float>(renderExtent.height);
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    //  Scissor
    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = renderExtent.width;
    scissor.extent.height = renderExtent.height;
    vkCmdSetScissor(cmd, 0, 1, &scissor);


    VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[2];
    descriptorBufferBindingInfo[0] = sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo();
    descriptorBufferBindingInfo[1] = environment->getCubemapDescriptorBuffer().getDescriptorBufferBindingInfo();
    vkCmdBindDescriptorBuffersEXT(cmd, 2, descriptorBufferBindingInfo);
    constexpr uint32_t sceneDataBufferIndex{0};
    constexpr uint32_t environmentIndex{1};
    constexpr VkDeviceSize zeroOffset{0};
    const VkDeviceSize environmentMapOffset{environment->getEnvironmentMapOffset(environmentMapindex)};

    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, environmentPipelineLayout, 0, 1, &sceneDataBufferIndex, &zeroOffset);
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, environmentPipelineLayout, 1, 1, &environmentIndex, &environmentMapOffset);

    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRendering(cmd);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void Engine::drawDeferredMrt(VkCommandBuffer cmd) const
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Deferred MRT Pass";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    VkClearValue clearValue = {0.0f, 0.0f};

    VkRenderingAttachmentInfo normalAttachment = vk_helpers::attachmentInfo(normalRenderTarget.imageView, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo albedoAttachment = vk_helpers::attachmentInfo(albedoRenderTarget.imageView, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo pbrAttachment = vk_helpers::attachmentInfo(pbrRenderTarget.imageView, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo velocityAttachment = vk_helpers::attachmentInfo(velocityRenderTarget.imageView, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = vk_helpers::attachmentInfo(depthImage.imageView, &clearValue, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.pNext = nullptr;

    VkRenderingAttachmentInfo deferredAttachments[4];
    deferredAttachments[0] = normalAttachment;
    deferredAttachments[1] = albedoAttachment;
    deferredAttachments[2] = pbrAttachment;
    deferredAttachments[3] = velocityAttachment;

    renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, renderExtent};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 4;
    renderInfo.pColorAttachments = deferredAttachments;
    renderInfo.pDepthAttachment = &depthAttachment;
    renderInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(cmd, &renderInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, deferredMrtPipeline);


    // Dynamic States
    {
        //  Viewport
        VkViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = static_cast<float>(renderExtent.width);
        viewport.height = static_cast<float>(renderExtent.height);
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        //  Scissor
        VkRect2D scissor = {};
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = renderExtent.width;
        scissor.extent.height = renderExtent.height;
        vkCmdSetScissor(cmd, 0, 1, &scissor);
    }

    if (!testRenderObject->canDraw()) {
        return;
    }

    VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[3];
    descriptorBufferBindingInfo[0] = sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo();
    descriptorBufferBindingInfo[1] = testRenderObject->getAddressesDescriptorBuffer().getDescriptorBufferBindingInfo();
    descriptorBufferBindingInfo[2] = testRenderObject->getTextureDescriptorBuffer().getDescriptorBufferBindingInfo();
    vkCmdBindDescriptorBuffersEXT(cmd, 3, descriptorBufferBindingInfo);

    constexpr VkDeviceSize zeroOffset{0};
    constexpr uint32_t sceneDataIndex{0};
    constexpr uint32_t addressIndex{1};
    constexpr uint32_t texturesIndex{2};
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, deferredMrtPipelineLayout, 0, 1, &sceneDataIndex, &zeroOffset);
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, deferredMrtPipelineLayout, 1, 1, &addressIndex, &zeroOffset);
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, deferredMrtPipelineLayout, 2, 1, &texturesIndex, &zeroOffset);

    constexpr VkDeviceSize offsets{0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &testRenderObject->getVertexBuffer().buffer, &offsets);
    vkCmdBindIndexBuffer(cmd, testRenderObject->getIndexBuffer().buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexedIndirect(cmd, testRenderObject->getIndirectBuffer().buffer, 0, testRenderObject->getDrawIndirectCommandCount(),
                             sizeof(VkDrawIndexedIndirectCommand));

    vkCmdEndDebugUtilsLabelEXT(cmd);

    vkCmdEndRendering(cmd);
}

// ReSharper disable once CppParameterMayBeConst
void Engine::drawDeferredResolve(VkCommandBuffer cmd) const
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Deferred Resolve Pass";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, deferredResolvePipeline);

    DeferredResolveData deferredResolveData;
    deferredResolveData.width = renderExtent.width;
    deferredResolveData.height = renderExtent.height;
    deferredResolveData.debug = deferredDebug;
    vkCmdPushConstants(cmd, deferredResolvePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(DeferredResolveData), &deferredResolveData);

    VkDescriptorBufferBindingInfoEXT deferredResolveBindingInfos[3]{};
    deferredResolveBindingInfos[0] = sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo();
    deferredResolveBindingInfos[1] = deferredResolveDescriptorBuffer.getDescriptorBufferBindingInfo();
    deferredResolveBindingInfos[2] = environment->getDiffSpecMapDescriptorBuffer().getDescriptorBufferBindingInfo();
    vkCmdBindDescriptorBuffersEXT(cmd, 3, deferredResolveBindingInfos);

    constexpr VkDeviceSize zeroOffset{0};
    constexpr uint32_t sceneDataIndex{0};
    constexpr uint32_t renderTargetsIndex{1};
    constexpr uint32_t environmentIndex{2};
    const VkDeviceSize environmentMapOffset{environment->getDiffSpecMapOffset(environmentMapindex)};
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, deferredResolvePipelineLayout, 0, 1, &sceneDataIndex, &zeroOffset);
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, deferredResolvePipelineLayout, 1, 1, &renderTargetsIndex, &zeroOffset);
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, deferredResolvePipelineLayout, 3, 1, &environmentIndex, &environmentMapOffset);


    const auto x = static_cast<uint32_t>(std::ceil(static_cast<float>(renderExtent.width) / 16.0f));
    const auto y = static_cast<uint32_t>(std::ceil(static_cast<float>(renderExtent.height) / 16.0f));
    vkCmdDispatch(cmd, x, y, 1);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

// ReSharper disable once CppParameterMayBeConst
void Engine::drawTaa(VkCommandBuffer cmd) const
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Compute TAA Pass";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, taaPipeline);

    TaaProperties taaProperties{};
    taaProperties.width = static_cast<int32_t>(renderExtent.width);
    taaProperties.height = static_cast<int32_t>(renderExtent.height);
    taaProperties.texelSize = {1.0f / static_cast<float>(renderExtent.width), 1.0f / static_cast<float>(renderExtent.height)};
    taaProperties.minBlend = taaMinBlend;
    taaProperties.maxBlend = taaMaxBlend;
    taaProperties.velocityWeight = taaVelocityRejectionWeight;
    taaProperties.depthWeight = taaDepthRejectionWeight;
    vkCmdPushConstants(cmd, taaPipelinelayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(TaaProperties), &taaProperties);

    VkDescriptorBufferBindingInfoEXT taaBindingInfo[1]{};
    taaBindingInfo[0] = taaDescriptorBuffer.getDescriptorBufferBindingInfo();
    vkCmdBindDescriptorBuffersEXT(cmd, 1, taaBindingInfo);

    constexpr VkDeviceSize zeroOffset{0};
    constexpr uint32_t taaDataIndex{0};
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, taaPipelinelayout, 0, 1, &taaDataIndex, &zeroOffset);

    const auto x = static_cast<uint32_t>(std::ceil(static_cast<float>(renderExtent.width) / 16.0f));
    const auto y = static_cast<uint32_t>(std::ceil(static_cast<float>(renderExtent.height) / 16.0f));
    vkCmdDispatch(cmd, x, y, 1);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void Engine::DEBUG_drawSpectate(VkCommandBuffer cmd) const
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Main Render Pass";
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
        VkClearValue colorClear = {0.0f, 0.0f};
        VkRenderingAttachmentInfo normalAttachment = vk_helpers::attachmentInfo(normalRenderTarget.imageView, &colorClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo albedoAttachment = vk_helpers::attachmentInfo(albedoRenderTarget.imageView, &colorClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo pbrAttachment = vk_helpers::attachmentInfo(pbrRenderTarget.imageView, &colorClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo velocityAttachment = vk_helpers::attachmentInfo(velocityRenderTarget.imageView, &colorClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkClearValue depthClearValue = {0.0f, 0.0f};

        VkRenderingAttachmentInfo depthAttachment = vk_helpers::attachmentInfo(depthImage.imageView, &depthClearValue, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
        VkRenderingInfo renderInfo{};
        renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderInfo.pNext = nullptr;

        VkRenderingAttachmentInfo deferredAttachments[4];
        deferredAttachments[0] = normalAttachment;
        deferredAttachments[1] = albedoAttachment;
        deferredAttachments[2] = pbrAttachment;
        deferredAttachments[3] = velocityAttachment;

        renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, renderExtent};
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = 4;
        renderInfo.pColorAttachments = deferredAttachments;
        renderInfo.pDepthAttachment = &depthAttachment;
        renderInfo.pStencilAttachment = nullptr;

        vkCmdBeginRendering(cmd, &renderInfo);


        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, deferredMrtPipeline);


        // Dynamic States
        {
            //  Viewport
            VkViewport viewport = {};
            viewport.x = 0;
            viewport.y = 0;
            float shrinkedWidth = static_cast<float>(renderExtent.width) / 3.0f;
            float shrinkedHeight = static_cast<float>(renderExtent.height) / 3.0f;
            viewport.width = shrinkedWidth;
            viewport.height = shrinkedHeight;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
            vkCmdSetViewport(cmd, 0, 1, &viewport);
            //  Scissor
            VkRect2D scissor = {};
            scissor.offset.x = 0;
            scissor.offset.y = 0;
            scissor.extent.width = static_cast<uint32_t>(shrinkedWidth);
            scissor.extent.height = static_cast<uint32_t>(shrinkedHeight);
            vkCmdSetScissor(cmd, 0, 1, &scissor);
        }

        if (!testRenderObject->canDraw()) {
            return;
        }

        VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[3];
        descriptorBufferBindingInfo[0] = spectateSceneDataDescriptorBuffer.getDescriptorBufferBindingInfo();
        descriptorBufferBindingInfo[1] = testRenderObject->getAddressesDescriptorBuffer().getDescriptorBufferBindingInfo();
        descriptorBufferBindingInfo[2] = testRenderObject->getTextureDescriptorBuffer().getDescriptorBufferBindingInfo();
        vkCmdBindDescriptorBuffersEXT(cmd, 3, descriptorBufferBindingInfo);

        constexpr VkDeviceSize zeroOffset{0};
        constexpr uint32_t sceneDataIndex{0};
        constexpr uint32_t addressIndex{1};
        constexpr uint32_t texturesIndex{2};

        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, deferredMrtPipelineLayout, 0, 1, &sceneDataIndex, &zeroOffset);
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, deferredMrtPipelineLayout, 1, 1, &addressIndex, &zeroOffset);
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, deferredMrtPipelineLayout, 2, 1, &texturesIndex, &zeroOffset);

        constexpr VkDeviceSize offsets{0};
        vkCmdBindVertexBuffers(cmd, 0, 1, &testRenderObject->getVertexBuffer().buffer, &offsets);
        vkCmdBindIndexBuffer(cmd, testRenderObject->getIndexBuffer().buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexedIndirect(cmd, testRenderObject->getIndirectBuffer().buffer, 0, testRenderObject->getDrawIndirectCommandCount(), sizeof(VkDrawIndexedIndirectCommand));

        vkCmdEndRendering(cmd);
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
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, deferredResolvePipeline);

        DeferredResolveData deferredResolveData;
        deferredResolveData.width = renderExtent.width;
        deferredResolveData.height = renderExtent.height;
        deferredResolveData.debug = 0;
        vkCmdPushConstants(cmd, deferredResolvePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(DeferredResolveData), &deferredResolveData);

        VkDescriptorBufferBindingInfoEXT deferredResolveBindingInfos[3]{};
        deferredResolveBindingInfos[0] = spectateSceneDataDescriptorBuffer.getDescriptorBufferBindingInfo();
        deferredResolveBindingInfos[1] = deferredResolveDescriptorBuffer.getDescriptorBufferBindingInfo();
        deferredResolveBindingInfos[2] = environment->getDiffSpecMapDescriptorBuffer().getDescriptorBufferBindingInfo();
        vkCmdBindDescriptorBuffersEXT(cmd, 3, deferredResolveBindingInfos);

        constexpr VkDeviceSize zeroOffset{0};
        constexpr uint32_t sceneDataIndex{0};
        constexpr uint32_t renderTargetsIndex{1};
        constexpr uint32_t environmentIndex{2};
        const VkDeviceSize environmentMapOffset{environment->getDiffSpecMapOffset(environmentMapindex)};
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, deferredResolvePipelineLayout, 0, 1, &sceneDataIndex, &zeroOffset);
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, deferredResolvePipelineLayout, 1, 1, &renderTargetsIndex, &zeroOffset);
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, deferredResolvePipelineLayout, 3, 1, &environmentIndex, &environmentMapOffset);


        const auto x = static_cast<uint32_t>(std::ceil(static_cast<float>(renderExtent.width) / 16.0f));
        const auto y = static_cast<uint32_t>(std::ceil(static_cast<float>(renderExtent.height) / 16.0f));
        vkCmdDispatch(cmd, x, y, 1);
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

    vkDeviceWaitIdle(device);


    delete environment;
    delete testGameObject1;
    delete testRenderObject;
    // destroy all other resources
    mainDeletionQueue.flush();

    // Destroy these after destroying all render objects
    destroyImage(whiteImage);
    destroyImage(errorCheckerboardImage);
    vkDestroySampler(device, defaultSamplerNearest, nullptr);
    vkDestroySampler(device, defaultSamplerLinear, nullptr);
    vkDestroyDescriptorSetLayout(device, emptyDescriptorSetLayout, nullptr);

    // ImGui
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(device, imguiPool, nullptr);

    // Main Rendering Command and Fence
    for (const auto& frame : frames) {
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
    destroyImage(drawImage);
    destroyImage(depthImage);

    destroyImage(normalRenderTarget);
    destroyImage(albedoRenderTarget);
    destroyImage(pbrRenderTarget);
    destroyImage(velocityRenderTarget);
    destroyImage(taaResolveBuffer);
    destroyImage(historyBuffer);
    destroyImage(depthHistoryBuffer);


    // Swapchain
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    for (const auto swapchainImageView : swapchainImageViews) {
        vkDestroyImageView(device, swapchainImageView, nullptr);
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


    std::vector<const char*> enabledInstanceExtensions;
    enabledInstanceExtensions.push_back("VK_EXT_debug_utils");

    // make the vulkan instance, with basic debug features
    auto inst_ret = builder.set_app_name("Will's Vulkan Renderer")
            .request_validation_layers(USE_VALIDATION_LAYERS)
            .use_default_debug_messenger()
            .require_api_version(1, 3)
            .enable_extensions(enabledInstanceExtensions)
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
    features12.runtimeDescriptorArray = true;
    features12.shaderSampledImageArrayNonUniformIndexing = true;

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
    createDrawResources(renderExtent.width, renderExtent.height);
}

void Engine::initCommands()
{
    const VkCommandPoolCreateInfo commandPoolInfo = vk_helpers::commandPoolCreateInfo(graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (auto& frame : frames) {
        VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &frame._commandPool));
        VkCommandBufferAllocateInfo cmdAllocInfo = vk_helpers::commandBufferAllocateInfo(frame._commandPool);
        VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &frame._mainCommandBuffer));
    }

    // Immediate Rendering
    VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &immCommandPool));
    const VkCommandBufferAllocateInfo immCmdAllocInfo = vk_helpers::commandBufferAllocateInfo(immCommandPool);
    VK_CHECK(vkAllocateCommandBuffers(device, &immCmdAllocInfo, &immCommandBuffer));
}

void Engine::initSyncStructures()
{
    const VkFenceCreateInfo fenceCreateInfo = vk_helpers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    const VkSemaphoreCreateInfo semaphoreCreateInfo = vk_helpers::semaphoreCreateInfo();

    for (auto& frame : frames) {
        VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &frame._renderFence));

        VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frame._swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frame._renderSemaphore));
    }

    // Immediate Rendeirng
    VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &immFence));
}

void Engine::initDefaultData() const
{
    // white image
    {
        const uint32_t white = packUnorm4x8(glm::vec4(1, 1, 1, 1));
        const AllocatedImage newImage = createImage(VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                                    false);
        destroyImage(newImage);
        whiteImage = createImage(&white, 4, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    }
    //checkerboard image
    {
        const uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
        const uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
        std::array<uint32_t, 16 * 16> pixels{}; //for 16x16 checkerboard texture
        for (int x = 0; x < 16; x++) {
            for (int y = 0; y < 16; y++) {
                pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
            }
        }
        errorCheckerboardImage = createImage(pixels.data(), 16 * 16 * 4, VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    }
    // default nearest sampler
    {
        VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sampl.magFilter = VK_FILTER_NEAREST;
        sampl.minFilter = VK_FILTER_NEAREST;
        sampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        vkCreateSampler(device, &sampl, nullptr, &defaultSamplerNearest);
    }
    // default linear sampler
    {
        VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sampl.magFilter = VK_FILTER_LINEAR;
        sampl.minFilter = VK_FILTER_LINEAR;
        sampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        vkCreateSampler(device, &sampl, nullptr, &defaultSamplerLinear);
    }
    // default "empty" descriptor set layout
    {
        DescriptorLayoutBuilder layoutBuilder;
        emptyDescriptorSetLayout = layoutBuilder.build(device,
                                                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
                                                       nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }
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
        return vkGetInstanceProcAddr(*(static_cast<VkInstance*>(vulkan_instance)), function_name);
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

void Engine::initDescriptors()
{
    //
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        sceneDataDescriptorSetLayout = layoutBuilder.build(
            device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
            , nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }

    sceneDataBuffer = createBuffer(sizeof(SceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    sceneDataDescriptorBuffer = DescriptorBufferUniform(instance, device, physicalDevice, allocator, sceneDataDescriptorSetLayout, 1);
    std::vector<DescriptorUniformData> sceneDataBuffers{1};
    sceneDataBuffers[0] = DescriptorUniformData{.uniformBuffer = sceneDataBuffer, .allocSize = sizeof(SceneData)};
    sceneDataDescriptorBuffer.setupData(device, sceneDataBuffers);


    mainDeletionQueue.pushFunction([&]() {
        vkDestroyDescriptorSetLayout(device, sceneDataDescriptorSetLayout, nullptr);
        destroyBuffer(sceneDataBuffer);
        sceneDataDescriptorBuffer.destroy(device, allocator);
    });


    spectateSceneDataBuffer = createBuffer(sizeof(SceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    spectateSceneDataDescriptorBuffer = DescriptorBufferUniform(instance, device, physicalDevice, allocator, sceneDataDescriptorSetLayout, 1);
    sceneDataBuffers[0] = DescriptorUniformData{.uniformBuffer = spectateSceneDataBuffer, .allocSize = sizeof(SceneData)};
    spectateSceneDataDescriptorBuffer.setupData(device, sceneDataBuffers);
    mainDeletionQueue.pushFunction([&]() {
        destroyBuffer(spectateSceneDataBuffer);
        spectateSceneDataDescriptorBuffer.destroy(device, allocator);
    });
}

void Engine::initPipelines()
{
    initFrustumCullingPipeline();
    initEnvironmentPipeline();
    initDeferredMrtPipeline();
    initDeferredResolvePipeline();
    initTaaPipeline();
}

void Engine::initFrustumCullingPipeline()
{
    assert(RenderObject::frustumCullingDescriptorSetLayout != VK_NULL_HANDLE);
    const VkDescriptorSetLayout layouts[2] =
    {
        sceneDataDescriptorSetLayout,
        RenderObject::frustumCullingDescriptorSetLayout,
    };

    VkPipelineLayoutCreateInfo frustumCullingPipelineLayoutCreateInfo{};
    frustumCullingPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    frustumCullingPipelineLayoutCreateInfo.pNext = nullptr;
    frustumCullingPipelineLayoutCreateInfo.pSetLayouts = layouts;
    frustumCullingPipelineLayoutCreateInfo.setLayoutCount = 2;
    frustumCullingPipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
    frustumCullingPipelineLayoutCreateInfo.pushConstantRangeCount = 0;

    VK_CHECK(vkCreatePipelineLayout(device, &frustumCullingPipelineLayoutCreateInfo, nullptr, &frustumCullingPipelineLayout));

    VkShaderModule computeShader;
    if (!vk_helpers::loadShaderModule("shaders/frustumCull.comp.spv", device, &computeShader)) {
        fmt::print("Error when building the compute shader (frustumCull.comp.spv)\n");
        abort();
    }

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.pNext = nullptr;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = computeShader;
    stageinfo.pName = "main"; // entry point in shader

    VkComputePipelineCreateInfo frustumCullingPipelineCreateInfo{};
    frustumCullingPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    frustumCullingPipelineCreateInfo.pNext = nullptr;
    frustumCullingPipelineCreateInfo.layout = frustumCullingPipelineLayout;
    frustumCullingPipelineCreateInfo.stage = stageinfo;
    frustumCullingPipelineCreateInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &frustumCullingPipelineCreateInfo, nullptr, &frustumCullingPipeline));

    vkDestroyShaderModule(device, computeShader, nullptr);

    mainDeletionQueue.pushFunction([&]() {
        vkDestroyPipelineLayout(device, frustumCullingPipelineLayout, nullptr);
        vkDestroyPipeline(device, frustumCullingPipeline, nullptr);
    });
}

void Engine::initEnvironmentPipeline()
{
    assert(Environment::layoutsCreated);

    const VkDescriptorSetLayout layouts[2] =
    {
        sceneDataDescriptorSetLayout,
        Environment::cubemapDescriptorSetLayout,
    };

    VkPipelineLayoutCreateInfo layout_info = vk_helpers::pipelineLayoutCreateInfo();
    layout_info.setLayoutCount = 2;
    layout_info.pSetLayouts = layouts;
    layout_info.pPushConstantRanges = nullptr;
    layout_info.pushConstantRangeCount = 0;

    VK_CHECK(vkCreatePipelineLayout(device, &layout_info, nullptr, &environmentPipelineLayout));

    VkShaderModule vertShader;
    if (!vk_helpers::loadShaderModule("shaders/environment.vert.spv", device, &vertShader)) {
        throw std::runtime_error("Error when building the vertex shader module(environment.vert.spv)");
    }
    VkShaderModule fragShader;
    if (!vk_helpers::loadShaderModule("shaders/environment.frag.spv", device, &fragShader)) {
        fmt::print("Error when building the fragment shader module(environment.frag.spv)\n");
    }

    PipelineBuilder renderPipelineBuilder;
    renderPipelineBuilder.setShaders(vertShader, fragShader);
    renderPipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    renderPipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    renderPipelineBuilder.disableMultisampling();
    renderPipelineBuilder.setupBlending(PipelineBuilder::BlendMode::NO_BLEND);
    renderPipelineBuilder.enableDepthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    renderPipelineBuilder.setupRenderer({drawImage.imageFormat}, depthImage.imageFormat);
    renderPipelineBuilder.setupPipelineLayout(environmentPipelineLayout);

    environmentPipeline = renderPipelineBuilder.buildPipeline(device, VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    vkDestroyShaderModule(device, vertShader, nullptr);
    vkDestroyShaderModule(device, fragShader, nullptr);

    mainDeletionQueue.pushFunction([&]() {
        vkDestroyPipelineLayout(device, environmentPipelineLayout, nullptr);
        vkDestroyPipeline(device, environmentPipeline, nullptr);
    });
}

void Engine::initDeferredMrtPipeline()
{
    assert(RenderObject::addressesDescriptorSetLayout != VK_NULL_HANDLE);
    assert(RenderObject::textureDescriptorSetLayout != VK_NULL_HANDLE);

    VkDescriptorSetLayout descriptorLayout[3];
    descriptorLayout[0] = sceneDataDescriptorSetLayout;
    descriptorLayout[1] = RenderObject::addressesDescriptorSetLayout;
    descriptorLayout[2] = RenderObject::textureDescriptorSetLayout;


    VkPipelineLayoutCreateInfo layoutInfo = vk_helpers::pipelineLayoutCreateInfo();
    layoutInfo.pSetLayouts = descriptorLayout;
    layoutInfo.setLayoutCount = 3;
    layoutInfo.pPushConstantRanges = nullptr;
    layoutInfo.pushConstantRangeCount = 0;

    VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &deferredMrtPipelineLayout));
    VkShaderModule vertShader;
    if (!vk_helpers::loadShaderModule("shaders/deferredMrt.vert.spv", device, &vertShader)) {
        throw std::runtime_error("Error when building the deferred vertex shader module(deferredMrt.vert.spv)\n");
    }
    VkShaderModule fragShader;
    if (!vk_helpers::loadShaderModule("shaders/deferredMrt.frag.spv", device, &fragShader)) {
        fmt::print("Error when building the deferred fragment shader module(deferredMrt.frag.spv)\n");
    }
    PipelineBuilder renderPipelineBuilder;
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

    renderPipelineBuilder.setShaders(vertShader, fragShader);
    renderPipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    renderPipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE); // VK_CULL_MODE_BACK_BIT
    renderPipelineBuilder.disableMultisampling();
    renderPipelineBuilder.setupBlending(PipelineBuilder::BlendMode::NO_BLEND);
    renderPipelineBuilder.enableDepthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    renderPipelineBuilder.setupRenderer(
        {normalRenderTarget.imageFormat, albedoRenderTarget.imageFormat, pbrRenderTarget.imageFormat, velocityRenderTarget.imageFormat}
        , depthImage.imageFormat);
    renderPipelineBuilder.setupPipelineLayout(deferredMrtPipelineLayout);

    deferredMrtPipeline = renderPipelineBuilder.buildPipeline(device, VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    vkDestroyShaderModule(device, vertShader, nullptr);
    vkDestroyShaderModule(device, fragShader, nullptr);

    mainDeletionQueue.pushFunction([&]() {
        vkDestroyPipelineLayout(device, deferredMrtPipelineLayout, nullptr);
        vkDestroyPipeline(device, deferredMrtPipeline, nullptr);
    });
}

void Engine::initDeferredResolvePipeline()
{
    //
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        layoutBuilder.addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        layoutBuilder.addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        layoutBuilder.addBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        deferredResolveRenderTargetLayout = layoutBuilder.build(device, VK_SHADER_STAGE_COMPUTE_BIT, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }
    //
    {
        std::vector<DescriptorImageData> renderTargetDescriptors;
        renderTargetDescriptors.reserve(5);
        const VkDescriptorImageInfo normalTarget{
            .sampler = defaultSamplerNearest, .imageView = normalRenderTarget.imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        const VkDescriptorImageInfo albedoTarget{
            .sampler = defaultSamplerNearest, .imageView = albedoRenderTarget.imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        const VkDescriptorImageInfo pbrDataTarget{
            .sampler = defaultSamplerNearest, .imageView = pbrRenderTarget.imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        const VkDescriptorImageInfo depthImageTarget{
            .sampler = defaultSamplerNearest, .imageView = depthImage.imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        const VkDescriptorImageInfo velocityTarget{
            .sampler = defaultSamplerNearest, .imageView = velocityRenderTarget.imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        const VkDescriptorImageInfo drawImageTarget{
            .imageView = drawImage.imageView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };
        renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, normalTarget, false}); // normal rt
        renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, albedoTarget, false}); // albedo rt
        renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, pbrDataTarget, false}); // pbr rt
        renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, depthImageTarget, false}); // depth image
        renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, velocityTarget, false}); // depth image
        renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, drawImageTarget, false}); // target image


        deferredResolveDescriptorBuffer = DescriptorBufferSampler(instance, device, physicalDevice, allocator, deferredResolveRenderTargetLayout, 1);
        deferredResolveDescriptorBuffer.setupData(device, renderTargetDescriptors);
    }

    VkPushConstantRange pushConstants;
    pushConstants.offset = 0;
    pushConstants.size = sizeof(DeferredResolveData);
    pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayout setLayouts[4];
    setLayouts[0] = sceneDataDescriptorSetLayout;
    setLayouts[1] = deferredResolveRenderTargetLayout;
    setLayouts[2] = emptyDescriptorSetLayout; // todo: lights
    setLayouts[3] = Environment::environmentMapDescriptorSetLayout;

    VkPipelineLayoutCreateInfo deferredResolvePipelineLayoutCreateInfo{};
    deferredResolvePipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    deferredResolvePipelineLayoutCreateInfo.pNext = nullptr;
    deferredResolvePipelineLayoutCreateInfo.pSetLayouts = setLayouts;
    deferredResolvePipelineLayoutCreateInfo.setLayoutCount = 4;
    deferredResolvePipelineLayoutCreateInfo.pPushConstantRanges = &pushConstants;
    deferredResolvePipelineLayoutCreateInfo.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(device, &deferredResolvePipelineLayoutCreateInfo, nullptr, &deferredResolvePipelineLayout));

    VkShaderModule computeShader;
    if (!vk_helpers::loadShaderModule("shaders/deferredResolve.comp.spv", device, &computeShader)) {
        fmt::print("Error when building the compute shader (deferredResolve.comp.spv)\n");
        abort();
    }

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.pNext = nullptr;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = computeShader;
    stageinfo.pName = "main"; // entry point in shader

    VkComputePipelineCreateInfo deferredResolvePipelineCreateInfo{};
    deferredResolvePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    deferredResolvePipelineCreateInfo.pNext = nullptr;
    deferredResolvePipelineCreateInfo.layout = deferredResolvePipelineLayout;
    deferredResolvePipelineCreateInfo.stage = stageinfo;
    deferredResolvePipelineCreateInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &deferredResolvePipelineCreateInfo, nullptr, &deferredResolvePipeline));

    vkDestroyShaderModule(device, computeShader, nullptr);

    mainDeletionQueue.pushFunction([&]() {
        vkDestroyDescriptorSetLayout(device, deferredResolveRenderTargetLayout, nullptr);
        deferredResolveDescriptorBuffer.destroy(device, allocator);
        vkDestroyPipelineLayout(device, deferredResolvePipelineLayout, nullptr);
        vkDestroyPipeline(device, deferredResolvePipeline, nullptr);
    });
}

void Engine::initTaaPipeline()
{
    //
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // drawImage
        layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // history
        layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // depthImage
        layoutBuilder.addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // depthHistory
        layoutBuilder.addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // velocity
        layoutBuilder.addBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // taa resolve buffer
        taaDescriptorSetLayout = layoutBuilder.build(device, VK_SHADER_STAGE_COMPUTE_BIT, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }
    //
    {
        std::vector<DescriptorImageData> taaDescriptors;
        taaDescriptors.reserve(4);
        const VkDescriptorImageInfo draw{
            .sampler = defaultSamplerNearest, .imageView = drawImage.imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        const VkDescriptorImageInfo history{
            .sampler = defaultSamplerNearest, .imageView = historyBuffer.imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        const VkDescriptorImageInfo depth{
            .sampler = defaultSamplerNearest, .imageView = depthImage.imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        const VkDescriptorImageInfo depthHistory{
            .sampler = defaultSamplerNearest, .imageView = depthHistoryBuffer.imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        const VkDescriptorImageInfo velocity{
            .sampler = defaultSamplerNearest, .imageView = velocityRenderTarget.imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        const VkDescriptorImageInfo taaResolve{
            .imageView = taaResolveBuffer.imageView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };
        taaDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, draw, false});
        taaDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, history, false});
        taaDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, depth, false});
        taaDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, depthHistory, false});
        taaDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, velocity, false});
        taaDescriptors.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, taaResolve, false});


        taaDescriptorBuffer = DescriptorBufferSampler(instance, device, physicalDevice, allocator, taaDescriptorSetLayout, 1);
        taaDescriptorBuffer.setupData(device, taaDescriptors);
    }

    VkPushConstantRange pushConstants{};
    pushConstants.offset = 0;
    pushConstants.size = sizeof(TaaProperties);
    pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayout setLayouts[1];
    setLayouts[0] = taaDescriptorSetLayout;

    VkPipelineLayoutCreateInfo taaPipelineLayoutCreateInfo{};
    taaPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    taaPipelineLayoutCreateInfo.pNext = nullptr;
    taaPipelineLayoutCreateInfo.pSetLayouts = setLayouts;
    taaPipelineLayoutCreateInfo.setLayoutCount = 1;
    taaPipelineLayoutCreateInfo.pPushConstantRanges = &pushConstants;
    taaPipelineLayoutCreateInfo.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(device, &taaPipelineLayoutCreateInfo, nullptr, &taaPipelinelayout));

    VkShaderModule computeShader;
    if (!vk_helpers::loadShaderModule("shaders/taa.comp.spv", device, &computeShader)) {
        fmt::print("Error when building the compute shader (taa.comp.spv)\n");
        abort();
    }

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.pNext = nullptr;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = computeShader;
    stageinfo.pName = "main"; // entry point in shader

    VkComputePipelineCreateInfo taaPipelienCreateInfo{};
    taaPipelienCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    taaPipelienCreateInfo.pNext = nullptr;
    taaPipelienCreateInfo.layout = taaPipelinelayout;
    taaPipelienCreateInfo.stage = stageinfo;
    taaPipelienCreateInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &taaPipelienCreateInfo, nullptr, &taaPipeline));

    vkDestroyShaderModule(device, computeShader, nullptr);

    mainDeletionQueue.pushFunction([&]() {
        vkDestroyDescriptorSetLayout(device, taaDescriptorSetLayout, nullptr);
        taaDescriptorBuffer.destroy(device, allocator);
        vkDestroyPipelineLayout(device, taaPipelinelayout, nullptr);
        vkDestroyPipeline(device, taaPipeline, nullptr);
    });
}

void Engine::initScene()
{
    // There is limit of 10!
    environment = new Environment(this);
    const std::filesystem::path envMapSource = "assets/environments";
    environment->loadEnvironment("Meadow", (envMapSource / "meadow_4k.hdr").string().c_str(), 0);
    //environment->loadCubemap((envMapSource / "wasteland_clouds_4k.hdr").string().c_str(), 1);
    environment->loadEnvironment("Wasteland", (envMapSource / "wasteland_clouds_puresky_4k.hdr").string().c_str(), 2);
    //environment->loadCubemap((envMapSource / "kloppenheim_06_puresky_4k.hdr").string().c_str(), 3);
    environment->loadEnvironment("Overcast Sky", (envMapSource / "kloofendal_overcast_puresky_4k.hdr").string().c_str(), 4);
    //environment->loadCubemap((envMapSource / "mud_road_puresky_4k.hdr").string().c_str(), 5);
    //environment->loadCubemap((envMapSource / "sunflowers_puresky_4k.hdr").string().c_str(), 6);
    environment->loadEnvironment("Sunset Sky", (envMapSource / "belfast_sunset_puresky_4k.hdr").string().c_str(), 7);

    //testRenderObject = new RenderObject{this, "assets/models/BoxTextured/glTF/BoxTextured.gltf"};
    //testRenderObject = new RenderObject{this, "assets/models/structure_mat.glb"};
    //testRenderObject = new RenderObject{this, "assets/models/structure.glb"};
    //testRenderObject = new RenderObject{this, "assets/models/Suzanne/glTF/Suzanne.gltf"};
    testRenderObject = new RenderObject{this, "assets/models/sponza2/Sponza.gltf"};
    testGameObject1 = testRenderObject->generateGameObject();
    testGameObject2 = testRenderObject->generateGameObject();
    testGameObject3 = testRenderObject->generateGameObject();
    testGameObject4 = testRenderObject->generateGameObject();
    testGameObject5 = testRenderObject->generateGameObject();
    scene.addGameObject(testGameObject1);
    scene.addGameObject(testGameObject2);
    scene.addGameObject(testGameObject3);
    scene.addGameObject(testGameObject4);
    scene.addGameObject(testGameObject5);

    testGameObject1->transform.setScale(1.f);
    testGameObject1->dirty();

    testGameObject2->transform.setScale(1.0f);
    testGameObject2->transform.translate(glm::vec3(100.0f, 0.0f, 0.0f));
    testGameObject2->dirty();

    testGameObject3->transform.setScale(1.0f);
    testGameObject3->transform.translate(glm::vec3(-100.0f, 0.0f, 0.0f));
    testGameObject3->dirty();

    testGameObject4->transform.setScale(1.0f);
    testGameObject4->transform.translate(glm::vec3(0.0f, -100.0f, 0.0f));
    testGameObject4->dirty();

    testGameObject5->transform.setScale(1.0f);
    testGameObject5->transform.translate(glm::vec3(0.0f, 100.0f, 0.0f));
    testGameObject5->dirty();
}

void Engine::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function) const
{
    VK_CHECK(vkResetFences(device, 1, &immFence));
    VK_CHECK(vkResetCommandBuffer(immCommandBuffer, 0));

    const auto cmd = immCommandBuffer;
    const VkCommandBufferBeginInfo cmdBeginInfo = vk_helpers::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
    function(cmd);
    VK_CHECK(vkEndCommandBuffer(cmd));

    const VkCommandBufferSubmitInfo cmdSubmitInfo = vk_helpers::commandBufferSubmitInfo(cmd);
    const VkSubmitInfo2 submitInfo = vk_helpers::submitInfo(&cmdSubmitInfo, nullptr, nullptr);

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

AllocatedBuffer Engine::createReceivingBuffer(size_t allocSize) const
{
    return createBuffer(allocSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU);
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
    const VkDeviceAddress srcPtr = vkGetBufferDeviceAddress(device, &addressInfo);
    return srcPtr;
}

void Engine::destroyBuffer(AllocatedBuffer& buffer) const
{
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
    buffer.buffer = VK_NULL_HANDLE;
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

AllocatedImage Engine::createImage(const void* data, const size_t dataSize, const VkExtent3D size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped /*= false*/) const
{
    const size_t data_size = dataSize;
    AllocatedBuffer uploadbuffer = createBuffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    memcpy(uploadbuffer.info.pMappedData, data, data_size);

    const AllocatedImage newImage = createImage(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

    immediateSubmit([&](VkCommandBuffer cmd) {
        vk_helpers::transitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

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
            vk_helpers::transitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
        }
    });

    destroyBuffer(uploadbuffer);

    return newImage;
}

AllocatedImage Engine::createCubemap(const VkExtent3D size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped) const
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
    allocinfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VK_CHECK(vmaCreateImage(allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

    VkImageViewCreateInfo view_info = vk_helpers::cubemapViewCreateInfo(format, newImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
    view_info.subresourceRange.levelCount = img_info.mipLevels;

    VK_CHECK(vkCreateImageView(device, &view_info, nullptr, &newImage.imageView));

    return newImage;
}

void Engine::destroyImage(const AllocatedImage& img) const
{
    vkDestroyImageView(device, img.imageView, nullptr);
    vmaDestroyImage(allocator, img.image, img.allocation);
}

void Engine::createSwapchain(const uint32_t width, const uint32_t height)
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
    for (const auto swapchainImage : swapchainImageViews) {
        vkDestroyImageView(device, swapchainImage, nullptr);
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
        vmaCreateImage(allocator, &renderImageInfo, &renderImageAllocationInfo, &drawImage.image, &drawImage.allocation, nullptr);

        VkImageViewCreateInfo rview_info = vk_helpers::imageviewCreateInfo(drawImage.imageFormat, drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
        VK_CHECK(vkCreateImageView(device, &rview_info, nullptr, &drawImage.imageView));
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
        vmaCreateImage(allocator, &depthImageInfo, &depthImageAllocationInfo, &depthImage.image, &depthImage.allocation, nullptr);

        VkImageViewCreateInfo depthViewInfo = vk_helpers::imageviewCreateInfo(depthImage.imageFormat, depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);
        VK_CHECK(vkCreateImageView(device, &depthViewInfo, nullptr, &depthImage.imageView));
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
            vmaCreateImage(allocator, &imageInfo, &renderImageAllocationInfo, &renderTarget.image, &renderTarget.allocation,
                           nullptr);

            const VkImageViewCreateInfo imageViewInfo = vk_helpers::imageviewCreateInfo(renderTarget.imageFormat, renderTarget.image,
                                                                                        VK_IMAGE_ASPECT_COLOR_BIT);
            VK_CHECK(vkCreateImageView(device, &imageViewInfo, nullptr, &renderTarget.imageView));

            return renderTarget;
        });

        normalRenderTarget = generateRenderTarget(VK_FORMAT_R8G8B8A8_SNORM);
        albedoRenderTarget = generateRenderTarget(VK_FORMAT_R8G8B8A8_UNORM);
        pbrRenderTarget = generateRenderTarget(VK_FORMAT_R8G8B8A8_UNORM);
        velocityRenderTarget = generateRenderTarget(VK_FORMAT_R16G16_SFLOAT);
    }
    // TAA Resolve
    {
        taaResolveBuffer.imageFormat = drawImageFormat;
        const VkExtent3D drawImageExtent = {renderExtent.width, renderExtent.height, 1};
        taaResolveBuffer.imageExtent = drawImageExtent;
        VkImageUsageFlags taaResolveUsages{};
        taaResolveUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
        taaResolveUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        taaResolveUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;

        const VkImageCreateInfo taaResolveImageInfo = vk_helpers::imageCreateInfo(taaResolveBuffer.imageFormat, taaResolveUsages, drawImageExtent);

        VmaAllocationCreateInfo taaResolveAllocationInfo = {};
        taaResolveAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        taaResolveAllocationInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vmaCreateImage(allocator, &taaResolveImageInfo, &taaResolveAllocationInfo, &taaResolveBuffer.image, &taaResolveBuffer.allocation, nullptr);

        const VkImageViewCreateInfo taaResolveImageViewInfo = vk_helpers::imageviewCreateInfo(taaResolveBuffer.imageFormat, taaResolveBuffer.image, VK_IMAGE_ASPECT_COLOR_BIT);
        VK_CHECK(vkCreateImageView(device, &taaResolveImageViewInfo, nullptr, &taaResolveBuffer.imageView));
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
        vmaCreateImage(allocator, &historyBufferImageInfo, &historyBufferAllocationInfo, &historyBuffer.image, &historyBuffer.allocation, nullptr);

        const VkImageViewCreateInfo historyBufferImageViewInfo = vk_helpers::imageviewCreateInfo(historyBuffer.imageFormat, historyBuffer.image, VK_IMAGE_ASPECT_COLOR_BIT);
        VK_CHECK(vkCreateImageView(device, &historyBufferImageViewInfo, nullptr, &historyBuffer.imageView));
    }
    // Depth History
    {
        depthHistoryBuffer.imageFormat = depthImageFormat;
        VkExtent3D depthImageExtent = {width, height, 1};
        depthHistoryBuffer.imageExtent = depthImageExtent;
        VkImageUsageFlags depthHistoryBufferUsages{};
        depthHistoryBufferUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
        depthHistoryBufferUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        VkImageCreateInfo depthImageInfo = vk_helpers::imageCreateInfo(depthHistoryBuffer.imageFormat, depthHistoryBufferUsages, depthImageExtent);

        VmaAllocationCreateInfo depthImageAllocationInfo = {};
        depthImageAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        depthImageAllocationInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vmaCreateImage(allocator, &depthImageInfo, &depthImageAllocationInfo, &depthHistoryBuffer.image, &depthHistoryBuffer.allocation, nullptr);

        VkImageViewCreateInfo depthViewInfo = vk_helpers::imageviewCreateInfo(depthHistoryBuffer.imageFormat, depthHistoryBuffer.image, VK_IMAGE_ASPECT_DEPTH_BIT);
        VK_CHECK(vkCreateImageView(device, &depthViewInfo, nullptr, &depthHistoryBuffer.imageView));
    }
}
