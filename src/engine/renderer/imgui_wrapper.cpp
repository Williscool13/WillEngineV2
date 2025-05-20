//
// Created by William on 2024-12-15.
//

#include "imgui_wrapper.h"

#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>

#include <Jolt/Jolt.h>


#include "environment/environment.h"
#include "pipelines/debug/debug_highlighter.h"
#include "pipelines/post/post_process/post_process_pipeline_types.h"
#include "pipelines/shadows/cascaded_shadow_map/cascaded_shadow_map.h"
#include "pipelines/shadows/ground_truth_ambient_occlusion/ambient_occlusion_types.h"
#include "pipelines/shadows/ground_truth_ambient_occlusion/ground_truth_ambient_occlusion_pipeline.h"
#include "engine/core/engine.h"
#include "engine/core/time.h"
#include "engine/core/camera/free_camera.h"
#include "engine/core/game_object/game_object_factory.h"
#include "engine/core/game_object/renderable.h"
#include "engine/core/scene/serializer.h"
#include "engine/physics/physics.h"
#include "engine/util/file.h"
#include "engine/util/math_utils.h"

namespace will_engine
{
ImguiWrapper::ImguiWrapper(const VulkanContext& context, const ImguiWrapperInfo& imguiWrapperInfo) : context(context)
{
    // DearImGui implementation, basically copied directly from the Vulkan/SDl2 from DearImGui samples.
    // Because this project uses VOLK, additionally need to load functions.
    // DYNAMIC RENDERING (NOT RENDER PASS)
    constexpr VkDescriptorPoolSize pool_sizes[] =
    {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 100;
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
    ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_3, [](const char* function_name, void* vulkan_instance) {
        return vkGetInstanceProcAddr(*(static_cast<VkInstance*>(vulkan_instance)), function_name);
    }, &instance);

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForVulkan(imguiWrapperInfo.window);
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
    if (currentlySelectedTextureImguiId != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(currentlySelectedTextureImguiId);
        currentlySelectedTextureImguiId = VK_NULL_HANDLE;
    }

    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(context.device, imguiPool, nullptr);
}

void ImguiWrapper::handleInput(const SDL_Event& e)
{
    ImGui_ImplSDL3_ProcessEvent(&e);
}

void ImguiWrapper::selectMap(Map* newMap)
{
    selectedMap = newMap;
    if (!selectedMap) {
        return;
    }

    auto terrainComponent = selectedMap->getComponent<components::TerrainComponent>();
    if (!terrainComponent) {
        auto& factory = components::ComponentFactory::getInstance();
        auto newComponent = factory.createComponent(components::TerrainComponent::TYPE, "Terrain Component");
        selectedMap->addComponent(std::move(newComponent));
    }

    terrainComponent = selectedMap->getComponent<components::TerrainComponent>();
    if (!terrainComponent) {
        fmt::print("Unable to create terrain component, very strange");
        return;
    }

    terrainGenerationSettings = terrainComponent->getTerrainGenerationProperties();
    terrainSeed = terrainComponent->getSeed();
    terrainConfig = terrainComponent->getConfig();
    if (terrain::TerrainChunk* terrainChunk = terrainComponent->getTerrainChunk()) {
        terrainProperties = terrainChunk->getTerrainProperties();
        terrainTextures = terrainChunk->getTerrainTextureIds();
    } else {
        terrainProperties = {};
        terrainTextures = {};
    }

}

void ImguiWrapper::imguiInterface(Engine* engine)
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (ImGui::Begin("Main")) {
        if (ImGui::BeginTabBar("Profiler")) {
            if (ImGui::BeginTabItem("Runtime")) {
                ImGui::Columns(2, "StartupTimers");

                ImGui::Text("Operation");
                ImGui::NextColumn();
                ImGui::Text("Time (ms)");
                ImGui::NextColumn();
                ImGui::Separator();

                for (const auto& [name, timer] : engine->profiler.getTimers()) {
                    std::string_view nameView = name;
                    if (!nameView.empty()) {
                        nameView.remove_prefix(1);
                    }

                    ImGui::Text("%s", nameView.data());
                    ImGui::NextColumn();
                    ImGui::Text("%.2f", timer.getAverageTime());
                    ImGui::NextColumn();
                }

                ImGui::Columns(1);
                ImGui::EndTabItem();
            }


            if (ImGui::BeginTabItem("Startup")) {
                const auto& entries = engine->startupProfiler.getEntries();

                ImGui::Columns(2, "StartupProfilerColumns", true);
                ImGui::SetColumnWidth(0, 400);
                ImGui::Text("Entry Name");
                ImGui::NextColumn();
                ImGui::Text("Time Diff (ms)");
                ImGui::NextColumn();
                ImGui::Separator();

                if (entries.empty()) {
                    ImGui::Text("No profiling data available");
                    ImGui::Columns(1);
                    return;
                }

                for (size_t i = 0; i < entries.size(); i++) {
                    ImGui::Text("%s", entries[i].name.c_str());
                    ImGui::NextColumn();

                    if (i == 0) {
                        ImGui::Text("0.00");
                    }
                    else {
                        auto diff = std::chrono::duration_cast<std::chrono::microseconds>(
                                        entries[i].time - entries[i - 1].time).count() / 1000.0f;
                        ImGui::Text("%.2f", diff);
                    }
                    ImGui::NextColumn();
                }

                ImGui::Columns(1);
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
    ImGui::End();


    if (ImGui::Begin("Settings Window")) {
        if (ImGui::BeginTabBar("Settings Tab")) {
            if (ImGui::BeginTabItem("General")) {
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::Button("Save All Settings")) {
                    Serializer::serializeEngineSettings(engine, EngineSettingsTypeFlag::ALL_SETTINGS);
                }

                ImGui::Separator();

                bool anySettingChanged = false;
                if (ImGui::Checkbox("Save Settings On Exit", &engine->engineSettings.saveOnExit)) {
                    anySettingChanged = true;
                }

                if (anySettingChanged) {
                    Serializer::serializeEngineSettings(engine, EngineSettingsTypeFlag::GENERAL_SETTINGS);
                }
                ImGui::Text("General settings above are always auto-saved");


                ImGui::Separator();

                ImGui::Text("Shortcuts");
                ImGui::Checkbox("Enable TAA", &engine->taaSettings.bEnabled);
                ImGui::Checkbox("Enable GTAO", &engine->gtaoSettings.bEnabled);
                ImGui::Checkbox("Enable Shadows", &engine->bEnableShadows);
                ImGui::Checkbox("Enable Contact Shadows", &engine->bEnableContactShadows);
                ImGui::Checkbox("Enable Transparent Primitives", &engine->bDrawTransparents);
                ImGui::Checkbox("Disable Physics", &engine->bEnablePhysics);
                ImGui::Checkbox("Enable Physics Debug", &engine->bDebugPhysics);
                ImGui::DragInt("Shadows PCF Level", &engine->csmSettings.pcfLevel, 2, 1, 5);


                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Camera")) {
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::Button("Save Camera Settings")) {
                    Serializer::serializeEngineSettings(engine, EngineSettingsTypeFlag::CAMERA_SETTINGS);
                }

                if (ImGui::Button("Reset Camera Position and Rotation")) {
                    engine->camera->setCameraTransform({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f});
                }

                if (ImGui::CollapsingHeader("Camera Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Indent();

                    glm::vec3 position = engine->camera->getTransform().getPosition();
                    ImGui::Text("Position: X: %.3f, Y: %.3f, Z: %.3f", position.x, position.y, position.z);

                    glm::vec3 forward = engine->camera->getForwardWS();
                    glm::vec3 up = engine->camera->getUpWS();
                    glm::vec3 right = engine->camera->getRightWS();

                    ImGui::Text("Forward: X: %.3f, Y: %.3f, Z: %.3f", forward.x, forward.y, forward.z);
                    ImGui::Text("Up: X: %.3f, Y: %.3f, Z: %.3f", up.x, up.y, up.z);
                    ImGui::Text("Right: X: %.3f, Y: %.3f, Z: %.3f", right.x, right.y, right.z);

                    ImGui::Separator();
                    float fov = glm::degrees(engine->camera->getFov());
                    float aspect = engine->camera->getAspectRatio();
                    float nearPlane = engine->camera->getNearPlane();
                    float farPlane = engine->camera->getFarPlane();

                    ImGui::Text("FOV: %.2f°", fov);
                    ImGui::Text("Aspect Ratio: %.3f", aspect);
                    ImGui::Text("Near Plane: %.3f", nearPlane);
                    ImGui::Text("Far Plane: %.3f", farPlane);

                    ImGui::Unindent();
                }

                if (ImGui::Button("Hard-Reset All Camera Settings")) {
                    delete engine->camera;
                    engine->camera = new FreeCamera();
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Lights")) {
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::Button("Save Light Settings")) {
                    Serializer::serializeEngineSettings(engine, EngineSettingsTypeFlag::LIGHT_SETTINGS);
                }

                if (ImGui::CollapsingHeader("Main Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
                    DirectionalLight currentMainLight = engine->getMainLight();
                    bool change = false;
                    float direction[3] = {currentMainLight.direction.x, currentMainLight.direction.y, currentMainLight.direction.z};
                    if (ImGui::DragFloat3("Direction", direction, 0.1)) {
                        currentMainLight.direction = glm::vec3(direction[0], direction[1], direction[2]);
                        change = true;
                    }
                    float color[3] = {currentMainLight.color.x, currentMainLight.color.y, currentMainLight.color.z};
                    if (ImGui::DragFloat3("Color", color, 0.1)) {
                        engine->mainLight.color = glm::vec3(color[0], color[1], color[2]);
                        change = true;
                    }
                    if (ImGui::DragFloat("Intensity", &engine->mainLight.intensity, 0.05f, 0.0f, 5.0f)) {
                        change = true;
                    }

                    if (change) {
                        currentMainLight.direction = normalize(currentMainLight.direction);
                        engine->setMainLight(currentMainLight);
                    }
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Temporal Anti-Aliasing")) {
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::Button("Save TAA Settings")) {
                    Serializer::serializeEngineSettings(engine, EngineSettingsTypeFlag::TEMPORAL_ANTIALIASING_SETTINGS);
                }
                ImGui::Checkbox("Enable TAA", &engine->taaSettings.bEnabled);
                ImGui::DragFloat("Taa Blend Value", &engine->taaSettings.blendValue, 0.01, 0.1f, 0.5f);

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Post-Processing")) {
                static bool tonemapping = (engine->postProcessData & post_process_pipeline::PostProcessType::Tonemapping) !=
                                          post_process_pipeline::PostProcessType::None;
                if (ImGui::CollapsingHeader("Tonemapping", ImGuiTreeNodeFlags_DefaultOpen)) {
                    if (ImGui::Checkbox("Enable Tonemapping", &tonemapping)) {
                        if (tonemapping) {
                            engine->postProcessData |= post_process_pipeline::PostProcessType::Tonemapping;
                        }
                        else {
                            engine->postProcessData &= ~post_process_pipeline::PostProcessType::Tonemapping;
                        }
                    }
                }
                if (ImGui::CollapsingHeader("Sharpening", ImGuiTreeNodeFlags_DefaultOpen)) {
                    static bool sharpening = (engine->postProcessData & post_process_pipeline::PostProcessType::Sharpening) !=
                                             post_process_pipeline::PostProcessType::None;
                    if (ImGui::Checkbox("Enable Sharpening", &sharpening)) {
                        if (sharpening) {
                            engine->postProcessData |= post_process_pipeline::PostProcessType::Sharpening;
                        }
                        else {
                            engine->postProcessData &= ~post_process_pipeline::PostProcessType::Sharpening;
                        }
                    }
                }
                if (ImGui::CollapsingHeader("FXAA", ImGuiTreeNodeFlags_DefaultOpen)) {
                    static bool fxaa = (engine->postProcessData & post_process_pipeline::PostProcessType::FXAA) !=
                                       post_process_pipeline::PostProcessType::None;
                    if (ImGui::Checkbox("Enable FXAA", &fxaa)) {
                        if (fxaa) {
                            engine->postProcessData |= post_process_pipeline::PostProcessType::FXAA;
                        }
                        else {
                            engine->postProcessData &= ~post_process_pipeline::PostProcessType::FXAA;
                        }
                    }
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Ambient Occlusion")) {
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::Button("Save AO Settings")) {
                    Serializer::serializeEngineSettings(engine, EngineSettingsTypeFlag::AMBIENT_OCCLUSION_SETTINGS);
                }

                ambient_occlusion::GTAOPushConstants& gtao = engine->gtaoSettings.pushConstants;
                const char* qualityPresets[] = {"Low", "Medium", "High", "Ultra"};
                int slicePreset = 0;

                if (gtao.sliceCount == ambient_occlusion::XE_GTAO_SLICE_COUNT_LOW) slicePreset = 0;
                else if (gtao.sliceCount == ambient_occlusion::XE_GTAO_SLICE_COUNT_MEDIUM) slicePreset = 1;
                else if (gtao.sliceCount == ambient_occlusion::XE_GTAO_SLICE_COUNT_HIGH) slicePreset = 2;
                else slicePreset = 3;

                if (ImGui::Combo("Slice Count Preset", &slicePreset, qualityPresets, IM_ARRAYSIZE(qualityPresets))) {
                    switch (slicePreset) {
                        case 0: gtao.sliceCount = ambient_occlusion::XE_GTAO_SLICE_COUNT_LOW;
                            break;
                        case 1: gtao.sliceCount = ambient_occlusion::XE_GTAO_SLICE_COUNT_MEDIUM;
                            break;
                        case 2: gtao.sliceCount = ambient_occlusion::XE_GTAO_SLICE_COUNT_HIGH;
                            break;
                        case 3: gtao.sliceCount = ambient_occlusion::XE_GTAO_SLICE_COUNT_ULTRA;
                            break;
                        default:
                            break;
                    }
                }

                int stepsPreset = 0;
                if (gtao.stepsPerSliceCount == ambient_occlusion::XE_GTAO_STEPS_PER_SLICE_COUNT_MEDIUM) stepsPreset = 1;
                else if (gtao.stepsPerSliceCount == ambient_occlusion::XE_GTAO_STEPS_PER_SLICE_COUNT_LOW) stepsPreset = 0;
                else if (gtao.stepsPerSliceCount == ambient_occlusion::XE_GTAO_STEPS_PER_SLICE_COUNT_ULTRA) stepsPreset = 3;
                else stepsPreset = 2;

                if (ImGui::Combo("Steps Per Slice Preset", &stepsPreset, qualityPresets, IM_ARRAYSIZE(qualityPresets))) {
                    switch (stepsPreset) {
                        case 0: gtao.stepsPerSliceCount = ambient_occlusion::XE_GTAO_STEPS_PER_SLICE_COUNT_LOW;
                            break;
                        case 1: gtao.stepsPerSliceCount = ambient_occlusion::XE_GTAO_STEPS_PER_SLICE_COUNT_MEDIUM;
                            break;
                        case 2: gtao.stepsPerSliceCount = ambient_occlusion::XE_GTAO_STEPS_PER_SLICE_COUNT_HIGH;
                            break;
                        case 3: gtao.stepsPerSliceCount = ambient_occlusion::XE_GTAO_STEPS_PER_SLICE_COUNT_ULTRA;
                            break;
                        default:
                            break;
                    }
                }

                ImGui::Separator();

                ImGui::SliderFloat("Effect Radius", &gtao.effectRadius, 0.1f, 2.0f);
                ImGui::SliderFloat("Effect Falloff Range", &gtao.effectFalloffRange, 0.0f, 1.0f);

                ImGui::Spacing();
                ImGui::Text("Denoise Parameters");
                ImGui::Separator();
                float blurBeta = gtao.denoiseBlurBeta;
                if (ImGui::SliderFloat("Denoise Blur Beta", &blurBeta, 0.0f, 5.0f)) {
                    if (ambient_occlusion::GTAO_DENOISE_PASSES != 0) {
                        gtao.denoiseBlurBeta = blurBeta;
                    }
                }
                ImGui::Checkbox("Final Denoise Pass", (bool*) &gtao.isFinalDenoisePass);

                ImGui::Spacing();
                ImGui::Text("Sampling Parameters");
                ImGui::Separator();
                ImGui::SliderFloat("Radius Multiplier", &gtao.radiusMultiplier, 0.1f, 3.0f);
                ImGui::SliderFloat("Sample Distribution Power", &gtao.sampleDistributionPower, 1.0f, 4.0f);
                ImGui::SliderFloat("Thin Occluder Compensation", &gtao.thinOccluderCompensation, 0.0f, 1.0f);
                ImGui::SliderFloat("Final Value Power", &gtao.finalValuePower, 1.0f, 4.0f);
                ImGui::SliderFloat("Depth Mip Sampling Offset", &gtao.depthMipSamplingOffset, 0.0f, 5.0f);

                ImGui::Spacing();


                ImGui::Spacing();
                ImGui::Text("Other Parameters");
                ImGui::Separator();
                ImGui::InputInt("Debug Mode", &gtao.debug);

                ImGui::Spacing();
                if (ImGui::Button("Reset to Defaults")) {
                    gtao = {};
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Screen Space Shadows")) {
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::Button("Save SSS Settings")) {
                    Serializer::serializeEngineSettings(engine, EngineSettingsTypeFlag::SCREEN_SPACE_SHADOWS_SETTINGS);
                }
                contact_shadows_pipeline::ContactShadowsPushConstants& sssPush = engine->sssSettings.pushConstants;
                ImGui::InputFloat("Surface Thickness", &sssPush.surfaceThickness);
                ImGui::InputFloat("Blinear Threshold", &sssPush.bilinearThreshold);
                ImGui::InputFloat("Shadow Contrast", &sssPush.shadowContrast);

                ImGui::SliderInt("Ignore Edge Pixels", &sssPush.bIgnoreEdgePixels, 0, 1);
                ImGui::SliderInt("Use Precision Offset", &sssPush.bUsePrecisionOffset, 0, 1);
                ImGui::SliderInt("Bilinear Offset Sampling Mode", &sssPush.bBilinearSamplingOffsetMode, 0, 1);

                ImGui::SliderInt("Contact Shadow Debug", &sssPush.debugMode, 0, 3);

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Shadows")) {
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::Button("Save CSM Settings")) {
                    Serializer::serializeEngineSettings(engine, EngineSettingsTypeFlag::CASCADED_SHADOW_MAP_SETTINGS);
                }

                ImGui::DragInt("CSM PCF Level", &engine->csmSettings.pcfLevel, 2, 1, 5);

                bool needUpdateCsmProperties = false;
                bool needRegenerateSplit = false;

                for (int32_t i = 0; i < cascaded_shadows::SHADOW_CASCADE_COUNT; ++i) {
                    ImGui::Text(fmt::format("Cascade {}:", i).c_str());
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(100);
                    if (ImGui::DragFloat(fmt::format("##linear{}", i).c_str(), &engine->csmSettings.cascadeBias[i].constant, 0.001f, 0.0f,
                                         1.0f, "%.3f")) {
                        needUpdateCsmProperties = true;
                    }
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(100);
                    if (ImGui::DragFloat(fmt::format("##slope{}", i).c_str(), &engine->csmSettings.cascadeBias[i].slope, 0.001f, 0.0f,
                                         1.0f, "%.3f")) {
                        needUpdateCsmProperties = true;
                    }
                }

                ImGui::Separator();

                if (ImGui::InputFloat("Split Lambda", &engine->csmSettings.splitLambda)) {
                    needUpdateCsmProperties = true;
                    needRegenerateSplit = true;
                }

                if (ImGui::InputFloat("Split Overlap", &engine->csmSettings.splitOverlap)) {
                    needUpdateCsmProperties = true;
                    needRegenerateSplit = true;
                }

                if (ImGui::InputFloat("Cascade Near Plane", &engine->csmSettings.cascadeNearPlane)) {
                    needUpdateCsmProperties = true;
                    needRegenerateSplit = true;
                }

                if (ImGui::InputFloat("Cascade Far Plane", &engine->csmSettings.cascadeFarPlane)) {
                    needUpdateCsmProperties = true;
                    needRegenerateSplit = true;
                }

                if (ImGui::Checkbox("Manual Splits", &engine->csmSettings.useManualSplit)) {
                    needUpdateCsmProperties = true;
                    needRegenerateSplit = true;
                }

                // todo: show auto split values, or if manual splits enabled, allow changing of manual split values

                if (needUpdateCsmProperties) {
                    engine->cascadedShadowMap->setCascadedShadowMapProperties(engine->csmSettings);
                }

                if (needRegenerateSplit) {
                    engine->cascadedShadowMap->generateSplits();
                }

                ImGui::SetNextItemWidth(100);
                ImGui::SliderInt("Shadow Map Level", &shadowMapDebug, 0, cascaded_shadows::SHADOW_CASCADE_COUNT - 1);
                ImGui::SameLine();
                if (ImGui::Button(fmt::format("Save Shadow Map", shadowMapDebug).c_str())) {
                    if (file::getOrCreateDirectory(file::imagesSavePath)) {
                        std::filesystem::path path = file::imagesSavePath / fmt::format("shadowMap{}.png", shadowMapDebug);

                        auto depthNormalize = [](const float depth) {
                            return logf(1.0f + depth * 15.0f) / logf(16.0f);
                        };

                        AllocatedImage shadowMap = engine->cascadedShadowMap->getShadowMap(shadowMapDebug);
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
                    }
                    else {
                        fmt::print(" Failed to save depth map image");
                    }
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Environment Map / Skybox")) {
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::Button("Save Environment Map Settings")) {
                    Serializer::serializeEngineSettings(engine, EngineSettingsTypeFlag::ENVIRONMENT_SETTINGS);
                }

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
                if (ImGui::Combo("Environment", &currentIndex, getLabel, &data, static_cast<int>(indexNamePairs.size()))) {
                    engine->environmentMapIndex = indexNamePairs[currentIndex].first;
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();


    if (ImGui::Begin("Renderer")) {
        if (ImGui::BeginTabBar("RendererTabs")) {
            if (ImGui::BeginTabItem("Debugging")) {
                ImGui::SetNextItemWidth(75.0f);
                if (ImGui::Button("Hot-Reload Shaders")) {
                    engine->hotReloadShaders();
                }


                ImGui::Separator();

                ImGui::Text("Deferred Debug");
                const char* deferredDebugOptions[]{
                    "None", "Depth", "Velocity", "Albedo", "Normal", "PBR", "Shadows", "Cascade Level", "nDotL", "AO", "Contact Shadows"
                };
                ImGui::Combo("Deferred Debug", &engine->deferredDebug, deferredDebugOptions, IM_ARRAYSIZE(deferredDebugOptions));
                ImGui::Separator();

                ImGui::Text("Frustum Cull Debug Draw");
                ImGui::Checkbox("Enable Frustum Cull Debug Draw", &engine->bEnableDebugFrustumCullDraw);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Save Images")) {
                if (ImGui::Button("Save Draw Image")) {
                    if (file::getOrCreateDirectory(file::imagesSavePath)) {
                        const std::filesystem::path path = file::imagesSavePath / "drawImage.png";
                        vk_helpers::saveImage(*engine->resourceManager, *engine->immediate, engine->drawImage,
                                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, vk_helpers::ImageFormat::RGBA16F, path.string());
                    }
                    else {
                        fmt::print(" Failed to find/create image save path directory");
                    }
                }

                if (ImGui::Button("Save Depth Image")) {
                    if (file::getOrCreateDirectory(file::imagesSavePath)) {
                        const std::filesystem::path path = file::imagesSavePath / "depthImage.png";
                        auto depthNormalize = [&engine](const float depth) {
                            const float zNear = engine->camera->getFarPlane();
                            const float zFar = engine->camera->getNearPlane() / 10.0;
                            float d = 1 - depth;
                            return (2.0f * zNear) / (zFar + zNear - d * (zFar - zNear));
                        };

                        vk_helpers::saveImageR32F(*engine->resourceManager, *engine->immediate, engine->depthStencilImage,
                                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT, path.string().c_str(),
                                                  depthNormalize);
                    }
                    else {
                        fmt::print(" Failed to find/create image save path directory");
                    }
                }

                if (ImGui::Button("Save Normals")) {
                    if (file::getOrCreateDirectory(file::imagesSavePath)) {
                        const std::filesystem::path path = file::imagesSavePath / "normalRT.png";
                        vk_helpers::saveImage(*engine->resourceManager, *engine->immediate, engine->normalRenderTarget,
                                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, vk_helpers::ImageFormat::A2R10G10B10_UNORM, path.string());
                    }
                    else {
                        fmt::print(" Failed to save normal render target");
                    }
                }

                if (ImGui::Button("Save Albedo Render Target")) {
                    if (file::getOrCreateDirectory(file::imagesSavePath)) {
                        const std::filesystem::path path = file::imagesSavePath / "albedoRT.png";
                        vk_helpers::saveImage(*engine->resourceManager, *engine->immediate, engine->albedoRenderTarget,
                                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, vk_helpers::ImageFormat::RGBA16F, path.string());
                    }
                    else {
                        fmt::print(" Failed to save albedo render target");
                    }
                }

                if (ImGui::Button("Save PBR Render Target")) {
                    if (file::getOrCreateDirectory(file::imagesSavePath)) {
                        std::filesystem::path path = file::imagesSavePath / "pbrRT.png";
                        vk_helpers::saveImage(*engine->resourceManager, *engine->immediate, engine->pbrRenderTarget,
                                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, vk_helpers::ImageFormat::RGBA8_UNORM, path.string());
                    }
                    else {
                        fmt::print(" Failed to save pbr render target");
                    }
                }

                if (ImGui::Button("Save Final Image")) {
                    if (file::getOrCreateDirectory(file::imagesSavePath)) {
                        std::filesystem::path path = file::imagesSavePath / "finalImage.png";
                        vk_helpers::saveImage(*engine->resourceManager, *engine->immediate, engine->finalImageBuffer, VK_IMAGE_LAYOUT_GENERAL,
                                              vk_helpers::ImageFormat::RGBA16F, path.string());
                    }
                    else {
                        fmt::print(" Failed to find/create image save path directory");
                    }
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Settings")) {
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    if (ImGui::Begin("Maps")) {
        static std::filesystem::path mapPath = {"assets/maps/sampleScene.willmap"};

        ImGui::Text("Map Source: %s", mapPath.empty() ? "None selected" : mapPath.string().c_str());
        ImGui::SameLine();
        if (ImGui::Button("...")) {
            IGFD::FileDialogConfig config;
            config.path = "./assets/maps/";
            config.fileName = mapPath.filename().string();
            IGFD::FileDialog::Instance()->OpenDialog(
                "WillmapDlg",
                "Load Willmap",
                ".willmap",
                config);
        }
        if (IGFD::FileDialog::Instance()->Display("WillmapDlg")) {
            if (IGFD::FileDialog::Instance()->IsOk()) {
                mapPath = IGFD::FileDialog::Instance()->GetFilePathName();
                mapPath = file::getRelativePath(mapPath);
            }

            IGFD::FileDialog::Instance()->Close();
        }

        bool alreadyExistsInActiveMaps{false};
        for (auto& map : engine->activeMaps) {
            if (map->getMapPath() == mapPath) {
                alreadyExistsInActiveMaps = true;
                break;
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(alreadyExistsInActiveMaps);
        if (ImGui::Button("Load")) {
            engine->assetManager->scanForAll();
            if (exists(mapPath)) {
                auto map = engine->createMap(mapPath);
                selectMap(map);
            }
            else {
                fmt::print("No map found at path %s", mapPath.string().c_str());
            }
        }
        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::BeginDisabled(!selectedMap);
        if (ImGui::Button("Save")) {
            if (selectedMap->saveMap(mapPath.string())) {
                ImGui::OpenPopup("SerializeSuccess");
            }
            else {
                ImGui::OpenPopup("SerializeError");
            }
        }

        // Success/Error popups
        if (ImGui::BeginPopupModal("SerializeSuccess", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Scene Serialization Success!");
            if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopupModal("SerializeError", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Scene Serialization Failed!");
            if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::EndDisabled();
    }
    ImGui::End();

    if (ImGui::Begin("Will Engine Type Generator")) {
        if (ImGui::BeginTabBar("SceneTabs")) {
            if (ImGui::BeginTabItem("Render Objects")) {
                if (ImGui::BeginTabBar("Render Object Tab Bar")) {
                    if (ImGui::BeginTabItem("Use")) {
                        ImGui::Text("Selected Render Object Details");

                        float detailsHeight = 160.0f;
                        ImGui::BeginChild("Selected Object", ImVec2(0, detailsHeight), ImGuiChildFlags_Borders);
                        RenderObject* _currentlySelected = engine->assetManager->getRenderObject(selectedRenderObjectId);

                        if (!_currentlySelected && engine->assetManager->hasAnyRenderObjects()) {
                            selectedRenderObjectId = engine->assetManager->getAnyRenderObject()->getId();
                        }

                        RenderObject* selectedRenderObject = engine->assetManager->getRenderObject(selectedRenderObjectId);
                        if (selectedRenderObject) {
                            ImGui::Text("Source: %s", file::getRelativePath(selectedRenderObject->getWillmodelPath()).string().c_str());
                            ImGui::Separator();
                            ImGui::Text("Name: %s", selectedRenderObject->getName().c_str());
                            ImGui::Text("GLTF Path: %s", file::getRelativePath(selectedRenderObject->getGltfPath()).string().c_str());
                            ImGui::Text("ID: %u", selectedRenderObject->getId());

                            bool isLoaded = selectedRenderObject->isLoaded();
                            if (isLoaded) {
                                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
                                static char objectName[256] = "";
                                ImGui::InputText("Object Name", objectName, sizeof(objectName));
                                ImGui::PopStyleColor();
                                if (ImGui::BeginTabBar("GameObject Generation")) {
                                    if (ImGui::BeginTabItem("Full Model")) {
                                        if (ImGui::Button("Generate Full Object")) {
                                            auto& gameObjectFactory = game_object::GameObjectFactory::getInstance();
                                            std::unique_ptr<game_object::GameObject> gameObject = gameObjectFactory.createGameObject(game_object::GameObject::getStaticType(), std::string(objectName));
                                            selectedRenderObject->generateMeshComponents(gameObject.get());
                                            selectedMap->addChild(std::move(gameObject));
                                            fmt::print("Added whole gltf model to the scene\n");
                                        }
                                        ImGui::EndTabItem();
                                    }

                                    if (ImGui::BeginTabItem("Single Mesh")) {
                                        static int selectedMeshIndex = 0;
                                        if (selectedMeshIndex >= selectedRenderObject->getMeshCount()) {
                                            selectedMeshIndex = 0;
                                        }

                                        ImGui::SetNextItemWidth(300.0f);
                                        if (ImGui::BeginCombo("Select Mesh", fmt::format("Mesh {}", selectedMeshIndex).c_str())) {
                                            for (size_t i = 0; i < selectedRenderObject->getMeshCount(); i++) {
                                                const bool isSelected = (selectedMeshIndex == static_cast<int>(i));
                                                if (ImGui::Selectable(fmt::format("Mesh {}", i).c_str(), isSelected)) {
                                                    selectedMeshIndex = static_cast<int>(i);
                                                }

                                                if (isSelected) {
                                                    ImGui::SetItemDefaultFocus();
                                                }
                                            }
                                            ImGui::EndCombo();
                                        }

                                        ImGui::SameLine();
                                        if (auto container = dynamic_cast<IComponentContainer*>(engine->selectedItem)) {
                                            if (ImGui::Button("Add to selected item")) {
                                                auto newComponent = components::ComponentFactory::getInstance().
                                                        createComponent(components::MeshRendererComponent::getStaticType(), "Mesh Renderer");
                                                components::Component* component = container->addComponent(std::move(newComponent));
                                                if (auto meshRenderer = dynamic_cast<components::MeshRendererComponent*>(component)) {
                                                    selectedRenderObject->generateMesh(meshRenderer, selectedMeshIndex);
                                                }
                                            }
                                        }
                                        else {
                                            ImGui::BeginDisabled(!selectedMap);
                                            if (ImGui::Button("Add to Scene")) {
                                                IHierarchical* gob = Engine::createGameObject(selectedMap, objectName);

                                                if (auto _container = dynamic_cast<IComponentContainer*>(gob)) {
                                                    auto newComponent = components::ComponentFactory::getInstance().createComponent(
                                                        components::MeshRendererComponent::getStaticType(), "Mesh Renderer");

                                                    components::Component* component = _container->addComponent(std::move(newComponent));
                                                    if (auto meshRenderer = dynamic_cast<components::MeshRendererComponent*>(component)) {
                                                        selectedRenderObject->generateMesh(meshRenderer, selectedMeshIndex);
                                                    }
                                                    fmt::print("Added single mesh to scene\n");
                                                }
                                            }
                                            ImGui::EndDisabled();
                                        }
                                        ImGui::EndTabItem();
                                    }

                                    ImGui::EndTabBar();
                                }
                            }
                            else {
                                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Load render object to see available meshes");
                            }
                        }


                        ImGui::EndChild();

                        ImGui::Text("Available Objects");
                        ImGui::SameLine();
                        if (ImGui::Button("Refresh")) {
                            engine->assetManager->scanForAll();
                        }

                        ImGui::BeginChild("Objects List", ImVec2(0, 0), ImGuiChildFlags_Borders);
                        for (const auto renderObject : engine->assetManager->getAllRenderObjects()) {
                            ImGui::PushID(renderObject->getId());

                            bool _isLoaded = renderObject->isLoaded();
                            bool checked = _isLoaded;

                            ImGui::Checkbox("##loaded", &checked);
                            if (checked && !_isLoaded) {
                                renderObject->load();
                            }

                            if (!checked && _isLoaded) {
                                renderObject->unload();
                            }

                            ImGui::SameLine();

                            bool isSelected = (selectedRenderObjectId == renderObject->getId());
                            if (ImGui::Selectable(renderObject->getName().c_str(), isSelected)) {
                                selectedRenderObjectId = renderObject->getId();
                            }

                            ImGui::PopID();
                        }
                        ImGui::EndChild();


                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem(".willmodel Generator")) {
                        static std::filesystem::path gltfPath;
                        static std::filesystem::path willmodelPath;

                        ImGui::Text("GLTF Source: %s", gltfPath.empty() ? "None selected" : gltfPath.string().c_str());

                        if (ImGui::Button("Select GLTF File")) {
                            IGFD::FileDialogConfig config;
                            config.path = "./assets/models";
                            IGFD::FileDialog::Instance()->OpenDialog(
                                "ChooseGLTFDlg",
                                "Choose GLTF File",
                                ".gltf,.glb",
                                config);
                        }

                        ImGui::Text("Output Path: %s", willmodelPath.empty() ? "None selected" : willmodelPath.string().c_str());

                        if (ImGui::Button("Select Output Path")) {
                            IGFD::FileDialogConfig config;
                            config.path = "./assets/willmodels";
                            config.fileName = willmodelPath.filename().string();
                            IGFD::FileDialog::Instance()->OpenDialog(
                                "SaveWillmodelDlg",
                                "Save Willmodel",
                                ".willmodel",
                                config);
                        }

                        if (IGFD::FileDialog::Instance()->Display("ChooseGLTFDlg")) {
                            if (IGFD::FileDialog::Instance()->IsOk()) {
                                gltfPath = IGFD::FileDialog::Instance()->GetFilePathName();
                                gltfPath = file::getRelativePath(gltfPath);

                                willmodelPath = gltfPath.parent_path() / gltfPath.filename().string();
                                willmodelPath = file::getRelativePath(willmodelPath);
                                willmodelPath.replace_extension(".willmodel");
                            }
                            IGFD::FileDialog::Instance()->Close();
                        }

                        if (IGFD::FileDialog::Instance()->Display("SaveWillmodelDlg")) {
                            if (IGFD::FileDialog::Instance()->IsOk()) {
                                willmodelPath = IGFD::FileDialog::Instance()->GetFilePathName();
                                willmodelPath = file::getRelativePath(willmodelPath);
                            }
                            IGFD::FileDialog::Instance()->Close();
                        }

                        if (!gltfPath.empty() && !willmodelPath.empty()) {
                            if (ImGui::Button("Compile Model")) {
                                if (Serializer::generateWillModel(gltfPath, willmodelPath)) {
                                    ImGui::OpenPopup("Success");
                                }
                                else {
                                    ImGui::OpenPopup("Error");
                                }
                            }
                        }

                        // Success/Error popups
                        if (ImGui::BeginPopupModal("Success", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                            ImGui::Text("Model compiled successfully!");
                            if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
                            ImGui::EndPopup();
                        }
                        if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                            ImGui::Text("Failed to compile model!");
                            if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
                            ImGui::EndPopup();
                        }

                        ImGui::EndTabItem();
                    }

                    ImGui::EndTabBar();
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Textures")) {
                if (ImGui::BeginTabBar("Textures Tab Bar")) {
                    if (ImGui::BeginTabItem("Visualization")) {
                        // Randomly select a texture if none selected
                        if (currentlySelectedTextureImguiId == VK_NULL_HANDLE) {
                            if (engine->assetManager->hasAnyTexture()) {
                                if (currentlySelectedTextureImguiId != VK_NULL_HANDLE) {
                                    vkDeviceWaitIdle(context.device);
                                    ImGui_ImplVulkan_RemoveTexture(currentlySelectedTextureImguiId);
                                    currentlySelectedTextureImguiId = VK_NULL_HANDLE;
                                }
                                Texture* randomTexture = engine->assetManager->getAnyTexture();

                                currentlySelectedTexture = randomTexture->getTextureResource();
                                currentlySelectedTextureImguiId = ImGui_ImplVulkan_AddTexture(
                                    engine->resourceManager->getDefaultSamplerLinear(), currentlySelectedTexture->getTexture().imageView,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                            }
                        }

                        std::string previewText;
                        if (currentlySelectedTexture) {
                            previewText = fmt::format("Texture ID - {}", currentlySelectedTexture->getId());
                        }
                        else {
                            previewText = "Select Texture";
                        }


                        if (ImGui::BeginCombo("Select Texture", previewText.c_str())) {
                            for (Texture* texture : engine->assetManager->getAllTextures()) {
                                bool isSelected = (currentlySelectedTexture->getId() == texture->getId());

                                std::string label = fmt::format("[{}] Texture ID - {}", texture->isTextureResourceLoaded() ? "LOADED" : "NOT LOADED",
                                                                texture->getId());

                                if (ImGui::Selectable(label.c_str(), isSelected)) {
                                    if (currentlySelectedTextureImguiId != VK_NULL_HANDLE) {
                                        vkDeviceWaitIdle(context.device);
                                        ImGui_ImplVulkan_RemoveTexture(currentlySelectedTextureImguiId);
                                        currentlySelectedTextureImguiId = VK_NULL_HANDLE;
                                    }

                                    currentlySelectedTexture = texture->getTextureResource();
                                    currentlySelectedTextureImguiId = ImGui_ImplVulkan_AddTexture(
                                        engine->resourceManager->getDefaultSamplerLinear(), currentlySelectedTexture->getTexture().imageView,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                                }
                            }

                            ImGui::EndCombo();
                        }

                        if (currentlySelectedTextureImguiId == VK_NULL_HANDLE) {
                            ImGui::Text("No Textures could be found");
                        }
                        else {
                            float maxSize = ImGui::GetContentRegionAvail().x;
                            maxSize = glm::min(maxSize, 512.0f);

                            VkExtent3D imageExtent = currentlySelectedTexture->getTexture().imageExtent;
                            float width = std::min(maxSize, static_cast<float>(imageExtent.width));
                            float aspectRatio = static_cast<float>(imageExtent.width) / static_cast<float>(imageExtent.height);
                            float height = width / aspectRatio;

                            ImGui::Image(reinterpret_cast<ImTextureID>(currentlySelectedTextureImguiId), ImVec2(width, height));
                        }

                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem(".willTexture Generator")) {
                        static std::filesystem::path texturesPath;

                        ImGui::Text("Will Texture folder to scan: %s", texturesPath.empty() ? "None selected" : texturesPath.string().c_str());

                        if (ImGui::Button("Select Folder to Scan")) {
                            IGFD::FileDialogConfig config;
                            config.path = "./assets/textures";
                            config.countSelectionMax = 1;
                            //config.flags = ImGuiFileDialogFlags_DontShowHiddenFiles;

                            IGFD::FileDialog::Instance()->OpenDialog(
                                "ChooseTextureFldrDlg",
                                "Choose Texture Folder",
                                nullptr, // No need for extensions when selecting directories
                                config);
                        }

                        if (IGFD::FileDialog::Instance()->Display("ChooseTextureFldrDlg")) {
                            if (IGFD::FileDialog::Instance()->IsOk()) {
                                texturesPath = IGFD::FileDialog::Instance()->GetCurrentPath();
                                texturesPath = file::getRelativePath(texturesPath);
                            }
                            IGFD::FileDialog::Instance()->Close();
                        }

                        ImGui::BeginDisabled(texturesPath.empty());
                        static int32_t generatedCount = 0;

                        static bool forceTextureGeneration = false;
                        ImGui::Checkbox("Forced", &forceTextureGeneration);
                        ImGui::SameLine();
                        if (ImGui::Button("Generate Texture Files")) {
                            generatedCount = 0;
                            if (!exists(texturesPath) || !is_directory(texturesPath)) {
                                fmt::print("Error: Invalid textures directory path: {}\n", texturesPath.string());
                                return;
                            }

                            std::vector<std::string> extensions = {".jpg", ".jpeg", ".png", ".tga", ".bmp"};

                            for (const auto& entry : std::filesystem::directory_iterator(texturesPath)) {
                                if (!entry.is_regular_file()) continue;

                                std::string extension = entry.path().extension().string();
                                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

                                if (std::find(extensions.begin(), extensions.end(), extension) != extensions.end()) {
                                    const std::filesystem::path texturePath = entry.path();
                                    const std::filesystem::path outputPath = texturePath.parent_path() /
                                                                             (texturePath.stem().string() + ".willtexture");

                                    if (!forceTextureGeneration && exists(outputPath)) {
                                        continue;
                                    }

                                    if (Serializer::generateWillTexture(texturePath, outputPath)) {
                                        generatedCount++;
                                    }
                                }
                            }

                            if (generatedCount > 0) {
                                ImGui::OpenPopup("Success");
                            }
                            else {
                                ImGui::OpenPopup("Error");
                            }
                        }

                        ImGui::EndDisabled();


                        // Success/Error popups
                        if (ImGui::BeginPopupModal("Success", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                            ImGui::Text(fmt::format("Successfully compiled {} textures into .willtexture files", generatedCount).c_str());
                            if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
                            ImGui::EndPopup();
                        }
                        if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                            if (forceTextureGeneration) {
                                ImGui::Text("Failed to find any textures to generate.");
                            }
                            else {
                                ImGui::Text("Failed to find any (new) textures to generate.");
                            }
                            if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
                            ImGui::EndPopup();
                        }
                        ImGui::EndTabItem();
                    }

                    ImGui::EndTabBar();
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    if (ImGui::Begin("Scene Graph")) {
        drawSceneGraph(engine);
    }
    ImGui::End();

    if (ImGui::Begin("Discardable Debug")) {
        ImGui::Checkbox("Draw Debug Render", &engine->bDrawDebugRendering);

        ImGui::Separator();
        if (ImGui::Button("Save Stencil Debug Draw")) {
            if (file::getOrCreateDirectory(file::imagesSavePath)) {
                const std::filesystem::path path = file::imagesSavePath / "debugStencil.png";
                vk_helpers::saveImage(*engine->resourceManager, *engine->immediate, engine->depthStencilImage,
                                      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, vk_helpers::ImageFormat::D32S8, path.string(), true);
            }
            else {
                fmt::print(" Failed to find/create image save path directory");
            }
        }
    }
    ImGui::End();

    if (engine->selectedItem) {
        if (IImguiRenderable* imguiRenderable = dynamic_cast<IImguiRenderable*>(engine->selectedItem)) {
            imguiRenderable->selectedRenderImgui();
        }
    }

    ImGui::Render();
}

void ImguiWrapper::drawSceneGraph(Engine* engine)
{
    static std::filesystem::path mapPath = {"assets/maps/newMap.willmap"};

    if (ImGui::Button("Create New Map")) {
        IGFD::FileDialogConfig config;
        config.path = "./assets/maps/";
        config.fileName = mapPath.filename().string();
        IGFD::FileDialog::Instance()->OpenDialog(
            "NewWillmapDlg",
            "Create New Map",
            ".willmap",
            config);
    }

    if (IGFD::FileDialog::Instance()->Display("NewWillmapDlg")) {
        if (IGFD::FileDialog::Instance()->IsOk()) {
            mapPath = IGFD::FileDialog::Instance()->GetFilePathName();
            if (Map* map = engine->createMap(mapPath)) {
                selectMap(map);
            }
        }
        IGFD::FileDialog::Instance()->Close();
    }


    if (ImGui::BeginCombo("Select Map", selectedMap ? selectedMap->getName().data() : "None")) {
        for (auto& map : engine->activeMaps) {
            bool isSelected = (selectedMap == map.get());
            if (ImGui::Selectable(map->getName().data(), isSelected)) {
                selectMap(map.get());
            }

            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (selectedMap == nullptr) {
        if (!engine->activeMaps.empty()) {
            const auto& firstMap = *engine->activeMaps.begin();
            if (firstMap) {
                selectMap(firstMap.get());
            }
        }
        if (selectedMap == nullptr)
        {
            ImGui::Text("No map currently selected");
            return;
        }
    }
    bool destroy = false;
    ImGui::SameLine();
    if (ImGui::Button("Destroy Map")) {
        destroy = true;
    }
    ImGui::Separator();

    if (ImGui::BeginTabBar("Scene Tab Bar")) {
        if (ImGui::BeginTabItem("Scene Graph")) {
            if (ImGui::Button("Create Game Object")) {
                static int32_t incrementId{0};
                [[maybe_unused]] IHierarchical* gameObject = Engine::createGameObject(selectedMap, fmt::format("New GameObject_{}", incrementId++));
            }
            ImGui::Separator();
            std::vector<IHierarchical*>& children = selectedMap->getChildren();
            if (!children.empty()) {
                for (IHierarchical* child : children) {
                    if (!displayGameObject(engine, child, 0)) {
                        break;
                    }
                }
            }
            else {
                ImGui::Text("Scene is empty");
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Terrain")) {
            ImGui::Checkbox("Draw Vertex Lines Only", &engine->bDrawTerrainLines);

            const auto currentTerrainComponent = selectedMap->getComponent<components::TerrainComponent>();
            ImGui::BeginDisabled(!currentTerrainComponent);
            if (ImGui::Button("Save Terrain as HeightMap")) {
                const std::vector<float> heightmapData = currentTerrainComponent->getHeightMapData();
                const std::filesystem::path path = file::imagesSavePath / "TerrainHeightMap.png";
                vk_helpers::saveHeightmap(heightmapData, NOISE_MAP_DIMENSIONS, NOISE_MAP_DIMENSIONS, path);
            }
            ImGui::EndDisabled();

            if (ImGui::BeginTabBar("Terrain Tab Bar")) {
                if (ImGui::BeginTabItem("Terrain Generation")) {
                    ImGui::Separator();

                    if (ImGui::CollapsingHeader("Noise Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
                        ImGui::SliderFloat("Scale", &terrainGenerationSettings.scale, 1.0f, 200.0f);
                        ImGui::SliderFloat("Persistence", &terrainGenerationSettings.persistence, 0.0f, 1.0f);
                        ImGui::SliderFloat("Lacunarity", &terrainGenerationSettings.lacunarity, 1.0f, 5.0f);
                        ImGui::SliderInt("Octaves", &terrainGenerationSettings.octaves, 1, 10);
                        ImGui::DragFloat2("Offset", &terrainGenerationSettings.offset.x, 0.1f);
                        ImGui::SliderFloat("Height Scale", &terrainGenerationSettings.heightScale, 1.0f, 200.0f);
                    }

                    ImGui::Separator();

                    if (ImGui::CollapsingHeader("Terrain Texture Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
                        ImGui::DragFloat2("UV Offset", &terrainConfig.uvOffset.x, 0.01f, -10.0f, 10.0f);
                        ImGui::DragFloat2("UV Scale", &terrainConfig.uvScale.x, 0.1f, 0.1f, 50.0f);
                        ImGui::Separator();
                        ImGui::Text("Material Settings");
                        ImGui::ColorEdit4("Base Color", &terrainConfig.baseColor.x, ImGuiColorEditFlags_NoAlpha);
                    }

                    ImGui::Separator();

                    ImGui::InputScalar("Seed", ImGuiDataType_U32, &terrainSeed);

                    ImGui::Separator();

                    static std::random_device rd{};
                    static std::seed_seq ss{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
                    static std::mt19937 gen(ss);
                    static std::uniform_int_distribution<uint32_t> dist;
                    if (ImGui::Button("Random Seed Generate")) {
                        terrainSeed = dist(gen);
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Generate Terrain", ImVec2(-1, 0))) {
                        currentTerrainComponent->generateTerrain(terrainGenerationSettings, terrainSeed, terrainConfig);
                        currentTerrainComponent->getTerrainChunk()->setTerrainBufferData(terrainProperties, terrainTextures);

                        terrainGenerationSettings = currentTerrainComponent->getTerrainGenerationProperties();
                        terrainSeed = currentTerrainComponent->getSeed();
                        terrainConfig = currentTerrainComponent->getConfig();
                        if (terrain::TerrainChunk* terrainChunk = currentTerrainComponent->getTerrainChunk()) {
                            terrainProperties = terrainChunk->getTerrainProperties();
                            terrainTextures = terrainChunk->getTerrainTextureIds();
                        } else {
                            terrainProperties = {};
                            terrainTextures = {};
                        }
                    }

                    if (ImGui::Button("Destroy Terrain", ImVec2(-1, 0))) {
                        currentTerrainComponent->destroyTerrain();
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Terrain Config")) {
                    if (ImGui::CollapsingHeader("Terrain Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
                        ImGui::SliderFloat("Rocks: Slope Threshold", &terrainProperties.slopeRockThreshold, 0.0f, 1.0f, "%.2f");
                        ImGui::SliderFloat("Rocks: Blend", &terrainProperties.slopeRockBlend, 0.0f, 1.0f, "%.2f");
                        ImGui::SliderFloat("Sand: Height Threshold", &terrainProperties.heightSandThreshold, 0.0f, 1.0f, "%.2f");
                        ImGui::SliderFloat("Sand: Blend", &terrainProperties.heightSandBlend, 0.0f, 1.0f, "%.2f");
                        ImGui::SliderFloat("Grass: Height Threshold", &terrainProperties.heightGrassThreshold, 0.0f, 1.0f, "%.2f");
                        ImGui::SliderFloat("Grass: Blend", &terrainProperties.heightGrassBlend, 0.0f, 1.0f, "%.2f");

                        ImGui::DragFloat("Min Height", &terrainProperties.minHeight, 1, -200, 200);
                        ImGui::DragFloat("Max Height", &terrainProperties.maxHeight, 1, -200, 200);
                    }

                    if (ImGui::CollapsingHeader("Terrain Textures", ImGuiTreeNodeFlags_DefaultOpen)) {
                        const char* textureSlotNames[3] = {"Grass Texture", "Rocks Texture", "Sand Texture"};

                        for (int i = 0; i < 3; i++) {
                            std::string currentTextureName = "None";
                            if (Texture* tex = engine->assetManager->getTexture(terrainTextures[i])) {
                                currentTextureName = tex->getName();
                                if (currentTextureName.empty()) {
                                    currentTextureName = std::to_string(tex->getId());
                                }
                            }

                            // Begin combo box
                            if (ImGui::BeginCombo(textureSlotNames[i], currentTextureName.c_str())) {
                                bool isSelected = (terrainTextures[i] == 0);
                                if (ImGui::Selectable("None", isSelected)) {
                                    terrainTextures[i] = 0;
                                }

                                std::vector<Texture*> textures = engine->assetManager->getAllTextures();

                                for (const Texture* tex2 : textures) {
                                    std::string textureName = tex2->getName();
                                    if (textureName.empty()) {
                                        textureName = std::to_string(tex2->getId());
                                    }

                                    isSelected = (tex2->getId() == terrainTextures[i]);
                                    if (ImGui::Selectable(textureName.c_str(), isSelected)) {
                                        terrainTextures[i] = tex2->getId();
                                    }

                                    if (isSelected) {
                                        ImGui::SetItemDefaultFocus();
                                    }
                                }
                                ImGui::EndCombo();
                            }
                        }
                    }

                    ImGui::Separator();

                    if (ImGui::Button("Update")) {
                        currentTerrainComponent->getTerrainChunk()->setTerrainBufferData(terrainProperties, terrainTextures);
                    }

                    ImGui::EndTabItem();
                }


                ImGui::EndTabBar();
            }


            ImGui::EndTabItem();
        }


        ImGui::EndTabBar();
    }


    if (destroy) {
        selectedMap->destroy();
        selectMap(nullptr);
        deselectItem(engine);
    }
}

bool ImguiWrapper::displayGameObject(Engine* engine, IHierarchical* obj, const int32_t depth)
{
    const int32_t indentLength = ImGui::GetFontSize();

    ImGui::PushID(obj);
    ImGui::Indent(static_cast<float>(depth * indentLength));

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_DefaultOpen;
    if (obj->getChildren().empty()) { flags |= ImGuiTreeNodeFlags_Leaf; }

    // Begin columns
    ImGui::Columns(3, "GameObjectColumns", false);

    // Column 1: Delete button
    ImGui::SetColumnWidth(0, 30.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
    if (ImGui::Button("X")) {
        if (engine->selectedItem == obj) {
            deselectItem(engine);
        }
        obj->destroy();
    }
    ImGui::PopStyleColor(2);
    ImGui::NextColumn();

    // Column 2: Name with TreeNode
    ImGui::SetColumnWidth(1, 300.0f);
    const std::string_view name = obj->getName();
    const float nameColumnWidth = ImGui::GetColumnWidth(1);
    const int maxNameLength = static_cast<int>(nameColumnWidth / ImGui::GetFontSize()) - depth;

    const std::string formattedName = maxNameLength < 0
                                          ? ""
                                          : name.length() > maxNameLength
                                                ? fmt::format("{:.{}s}...", name, std::max(0, maxNameLength - 3))
                                                : fmt::format("{:<{}}", name, maxNameLength);

    if (obj == engine->selectedItem) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.0f, 0.6f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.0f, 0.7f, 0.0f, 1.0f));
    }

    const bool isOpen = ImGui::TreeNodeEx("##TreeNode", flags, "%s", formattedName.c_str());

    if (obj == engine->selectedItem) {
        ImGui::PopStyleColor(3);
    }

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        if (obj == engine->selectedItem) {
            deselectItem(engine);
        }
        else {
            selectItem(engine, obj);
        }
    }
    ImGui::NextColumn();

    bool exit = false;
    // Column 3: Control buttons
    if (IHierarchical* parent = obj->getParent()) {
        constexpr float spacing = 5.0f;
        constexpr float buttonWidth = 60.0f;

        ImGui::BeginDisabled(parent == selectedMap);
        if (ImGui::Button("Undent", ImVec2(buttonWidth, 0))) {
            undent(obj);
            exit = true;
        }
        ImGui::EndDisabled();

        ImGui::SameLine(0, spacing);
        const std::vector<IHierarchical*>& parentChildren = parent->getChildren();
        ImGui::BeginDisabled(parent != obj->getParent() || parentChildren[0] == obj);
        if (ImGui::Button("Indent", ImVec2(buttonWidth, 0))) {
            indent(obj);
            exit = true;
        }
        ImGui::EndDisabled();

        ImGui::SameLine(0, spacing);
        ImGui::BeginDisabled(parent != obj->getParent() || parentChildren[0] == obj);
        if (ImGui::ArrowButton("##Up", ImGuiDir_Up)) {
            moveObject(obj, -1);
            exit = true;
        }
        ImGui::EndDisabled();

        ImGui::SameLine(0, spacing);
        ImGui::BeginDisabled(parent != obj->getParent() || parentChildren[parentChildren.size() - 1] == obj);
        if (ImGui::ArrowButton("##Down", ImGuiDir_Down)) {
            moveObject(obj, 1);
            exit = true;
        }
        ImGui::EndDisabled();
    }
    ImGui::NextColumn();

    ImGui::Columns(1);


    if (const auto imguiRenderable = dynamic_cast<IImguiRenderable*>(obj)) {
        imguiRenderable->renderImgui();
    }

    if (isOpen) {
        if (!exit) {
            for (IHierarchical* child : obj->getChildren()) {
                if (!displayGameObject(engine, child, depth + 1)) {
                    exit = true;
                }
            }
        }
        ImGui::TreePop();
    }


    ImGui::Unindent(static_cast<float>(depth * indentLength));
    ImGui::PopID();
    return !exit;
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

void ImguiWrapper::indent(IHierarchical* obj)
{
    IHierarchical* currParent = obj->getParent();
    if (currParent == nullptr) { return; }
    // get index in scene Root
    const std::vector<IHierarchical*>& currChildren = currParent->getChildren();
    const int32_t index = getIndexInVector(obj, currChildren);
    if (index == -1 || index == 0) { return; }

    IHierarchical* const newParent = currChildren[index - 1];
    currParent->moveChild(obj, newParent);
}

void ImguiWrapper::undent(IHierarchical* obj)
{
    IHierarchical* currentParent = obj->getParent();
    if (currentParent == nullptr) { return; }

    // Do not go beyond scene root
    IHierarchical* parentParent = currentParent->getParent();
    if (currentParent->getParent() == nullptr) { return; }

    // Get index of old parent under their parent
    const std::vector<IHierarchical*>& parentParentChildren = parentParent->getChildren();
    const int32_t parentIndex = getIndexInVector(currentParent, parentParentChildren);
    if (parentIndex == -1) { return; }

    // the count before the move
    const int32_t currentChildCount = parentParentChildren.size();

    currentParent->moveChild(obj, parentParent);


    // Already at end, no need to reorder.
    if (parentIndex == currentChildCount - 1) { return; }

    // Re-order to right below previous parent.
    parentParent->moveChildToIndex(currentChildCount, parentIndex + 1);
}

void ImguiWrapper::moveObject(const IHierarchical* obj, int diff)
{
    if (diff == 0) { return; }

    IHierarchical* parent = obj->getParent();
    if (!parent) { return; }

    const std::vector<IHierarchical*>& parentChildren = parent->getChildren();
    const int32_t index = getIndexInVector(obj, parentChildren);
    if (index == -1) { return; }

    const int32_t newIndex = index + diff;
    if (newIndex < 0 || newIndex >= static_cast<int32_t>(parentChildren.size())) { return; }

    parent->moveChildToIndex(index, newIndex);
}

int ImguiWrapper::getIndexInVector(const IHierarchical* obj, const std::vector<IHierarchical*>& vector)
{
    for (int32_t i = 0; i < vector.size(); i++) {
        if (vector[i] == obj) {
            return i;
        }
    }

    return -1;
}

void ImguiWrapper::selectItem(Engine* engine, IHierarchical* hierarchical)
{
#if WILL_ENGINE_DEBUG_DRAW
    engine->selectItem(hierarchical);
#endif
}

void ImguiWrapper::deselectItem(Engine* engine)
{
#if WILL_ENGINE_DEBUG_DRAW
    engine->deselectItem();
#endif
}
}
