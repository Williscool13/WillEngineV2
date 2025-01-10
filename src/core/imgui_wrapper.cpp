//
// Created by William on 2024-12-15.
//

#include "imgui_wrapper.h"

#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include <src/renderer/lighting/shadows/cascaded_shadow_map.h>

#include "engine.h"
#include "input.h"
#include "camera/camera.h"
#include "src/game/player/player_character.h"
#include "src/renderer/vk_helpers.h"
#include "src/renderer/vulkan_context.h"
#include "src/util/file_utils.h"
#include "src/util/time_utils.h"

ImguiWrapper::ImguiWrapper(const VulkanContext& context, const ImguiWrapperInfo& imguiWrapperInfo) : context(context)
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
    VK_CHECK(vkCreateDescriptorPool(context.device, &pool_info, nullptr, &imguiPool));

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls

    ImGui::StyleColorsLight();

    VkInstance instance = context.instance;
    // Need to LoadFunction when using VOLK/using VK_NO_PROTOTYPES
    ImGui_ImplVulkan_LoadFunctions([](const char* function_name, void* vulkan_instance) {
        return vkGetInstanceProcAddr(*(static_cast<VkInstance*>(vulkan_instance)), function_name);
    }, &instance);

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForVulkan(imguiWrapperInfo.window);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = context.instance;
    init_info.PhysicalDevice = context.physicalDevice;
    init_info.Device = context.device;
    init_info.QueueFamily = context.graphicsQueueFamily;
    init_info.Queue = context.graphicsQueue;
    init_info.DescriptorPool = imguiPool;
    init_info.Subpass = 0;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    //dynamic rendering parameters for imgui to use
    init_info.UseDynamicRendering = true;
    init_info.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &imguiWrapperInfo.swapchainImageFormat;


    ImGui_ImplVulkan_Init(&init_info);
    ImGui_ImplVulkan_CreateFontsTexture();
}

ImguiWrapper::~ImguiWrapper()
{
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(context.device, imguiPool, nullptr);
}

void ImguiWrapper::handleInput(const SDL_Event& e)
{
    ImGui_ImplSDL2_ProcessEvent(&e);
}

void ImguiWrapper::imguiInterface(Engine* engine)
{
    Input& input = Input::Get();
    TimeUtils& time = TimeUtils::Get();

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    if (ImGui::Begin("Main")) {
        ImGui::Text("Physics Time: %.2f ms", engine->physicsTime);
        ImGui::Text("Game Time: %.2f ms", engine->gameTime);
        ImGui::Text("Render Time: %.2f ms", engine->renderTime);

        ImGui::Text("Frame Time: %.2f ms", engine->frameTime);
        ImGui::Text("Delta Time: %.2f ms", time.getDeltaTime() * 1000.0f);

        ImGui::Separator();
        ImGui::Text("TAA Properties:");
        ImGui::Checkbox("TAA", &engine->bEnableTaa);
        ImGui::SameLine();
        ImGui::Checkbox("Jitter", &engine->bEnableJitter);
        ImGui::SameLine();
        if (ImGui::Button(engine->bEnableTaa && engine->bEnableJitter ? "Disable Both" : "Enable Both")) {
            bool newState = !(engine->bEnableTaa && engine->bEnableJitter);
            engine->bEnableTaa = newState;
            engine->bEnableJitter = newState;
        }

        if (engine->bEnableTaa) {
            ImGui::SetNextItemWidth(100);
            ImGui::InputFloat("TAA Blend", &engine->taaBlend);
            ImGui::Text("Taa Debug View");
            ImGui::SetNextItemWidth(100);
            const char* taaDebugLabels[] = {"None", "Velocity", "Validity", "-", "-", "-"};
            ImGui::Combo("TAA Debug View", &engine->taaDebug, taaDebugLabels, IM_ARRAYSIZE(taaDebugLabels));
        }
        ImGui::Separator();
        ImGui::Checkbox("Enable Shadow Map Debug", &engine->bEnableShadowMapDebug);
        ImGui::Checkbox("Enable Frustum Culling", &engine->bEnableFrustumCulling);
        ImGui::Separator();

        if (ImGui::TreeNode("Post-Process Effects")) {
            auto flags = static_cast<uint32_t>(engine->postProcessFlags);
            if (ImGui::CheckboxFlags("Tonemapping", &flags, 1)) {
                engine->postProcessFlags = static_cast<PostProcessType>(flags);
            }
            if (ImGui::CheckboxFlags("Sharpening", &flags, 2)) {
                engine->postProcessFlags = static_cast<PostProcessType>(flags);
            }
            ImGui::TreePop();
        }


        ImGui::Separator();
        const char* debugLabels[] = {"None", "Velocity Buffer", "Depth Buffer"};
        ImGui::Text("Debug View");
        ImGui::SetNextItemWidth(100);
        ImGui::Combo("Debug View", &engine->deferredDebug, debugLabels, IM_ARRAYSIZE(debugLabels));
    }
    ImGui::End();

    if (ImGui::Begin("Camera")) {
        const Camera* camera = engine->player->getCamera();

        glm::vec3 viewDir = camera->getForwardWS();
        glm::vec3 cameraRotation = camera->transform.getEulerAngles();
        cameraRotation = glm::degrees(cameraRotation);
        glm::vec3 cameraPosition = camera->transform.getPosition();
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

    if (ImGui::Begin("Save Final Image")) {
        if (ImGui::Button("Save Draw Image")) {
            if (file_utils::getOrCreateDirectory(file_utils::imagesSavePath)) {
                std::filesystem::path path = file_utils::imagesSavePath / "drawImage.png";
                vk_helpers::saveImageRGBA16SFLOAT(*engine->resourceManager, *engine->immediate, engine->postProcessOutputBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
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
                vk_helpers::saveImageR32F(*engine->resourceManager, *engine->immediate, engine->depthImage, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT,
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

                vk_helpers::savePacked32Bit(*engine->resourceManager, *engine->immediate, engine->normalRenderTarget, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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

                vk_helpers::savePacked32Bit(*engine->resourceManager, *engine->immediate, engine->albedoRenderTarget, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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
                vk_helpers::savePacked32Bit(*engine->resourceManager, *engine->immediate, engine->pbrRenderTarget, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                            VK_IMAGE_ASPECT_COLOR_BIT, path.string().c_str(), unpackFunc);
            } else {
                fmt::print(" Failed to save pbr render target");
            }
        }

        if (ImGui::Button("Save Post Process Resolve Target")) {
            if (file_utils::getOrCreateDirectory(file_utils::imagesSavePath)) {
                if (file_utils::getOrCreateDirectory(file_utils::imagesSavePath)) {
                    std::filesystem::path path = file_utils::imagesSavePath / "postProcesResolve.png";
                    vk_helpers::saveImageRGBA16SFLOAT(*engine->resourceManager, *engine->immediate, engine->postProcessOutputBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
                                                      path.string().c_str());
                } else {
                    fmt::print(" Failed to find/create image save path directory");
                }
            }
        }


        ImGui::Checkbox("Perspective Bounds", &engine->bShowPerspectiveBounds);
    }
    ImGui::End();

    if (ImGui::Begin("Maps")) {
        // Shadow Map Controls
        {
            ImGui::SetNextItemWidth(100);
            ImGui::SliderInt("Shadow Map Level", &engine->shadowMapDebug, 0, SHADOW_CASCADE_COUNT);
            ImGui::SameLine();
            if (ImGui::Button(fmt::format("Save Shadow Map", engine->shadowMapDebug).c_str())) {
                if (file_utils::getOrCreateDirectory(file_utils::imagesSavePath)) {
                    std::filesystem::path path = file_utils::imagesSavePath /
                                                 fmt::format("shadowMap{}.png", engine->shadowMapDebug);

                    auto depthNormalize = [](const float depth) {
                        return logf(1.0f + depth * 15.0f) / logf(16.0f);
                    };

                    AllocatedImage shadowMap = engine->cascadedShadowMap->getShadowMap(engine->shadowMapDebug);
                    if (shadowMap.image != VK_NULL_HANDLE) {
                        vk_helpers::saveImageR32F(
                            *engine->resourceManager,
                            *engine->immediate,
                            shadowMap,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            VK_IMAGE_ASPECT_DEPTH_BIT,
                            path.string().c_str(),
                            depthNormalize
                        );
                    }
                } else {
                    fmt::print(" Failed to save depth map image");
                }
            }
        }

        // Environment Map Selection
        {
            const auto& activeEnvironmentMapNames = engine->environmentMap->getActiveEnvironmentMapNames();

            std::vector<std::pair<int32_t, std::string> > indexNamePairs;
            for (const auto& [index, name] : activeEnvironmentMapNames) {
                indexNamePairs.emplace_back(index, name);
            }
            std::sort(indexNamePairs.begin(), indexNamePairs.end());

            auto it = std::ranges::find_if(indexNamePairs, [this, engine](const auto& pair) {
                return pair.first == engine->environmentMapIndex;
            });
            int currentIndex = (it != indexNamePairs.end()) ? static_cast<int>(std::distance(indexNamePairs.begin(), it)) : 0;

            struct ComboData
            {
                const std::vector<std::pair<int32_t, std::string> >* pairs;
            };

            auto getLabel = [](void* data, int idx, const char** out_text) -> bool {
                static std::string label;
                const auto& pairs = *static_cast<const ComboData*>(data)->pairs;
                label = pairs[idx].second;
                *out_text = label.c_str();
                return true;
            };

            ComboData data{&indexNamePairs};
            ImGui::SetNextItemWidth(250);
            if (ImGui::Combo("Select Environment Map", &currentIndex, getLabel,
                             &data, static_cast<int>(indexNamePairs.size()))) {
                engine->environmentMapIndex = indexNamePairs[currentIndex].first;
            }
        }
    }
    ImGui::End();

    engine->scene.imguiSceneGraph();

    ImGui::Render();
}

void ImguiWrapper::drawImgui(VkCommandBuffer cmd, const VkImageView targetImageView, const VkExtent2D swapchainExtent)
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
