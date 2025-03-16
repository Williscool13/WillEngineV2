//
// Created by William on 2024-12-15.
//

#include "imgui_wrapper.h"

#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>

#include <Jolt/Jolt.h>


#include "environment/environment.h"
#include "lighting/shadows/cascaded_shadow_map.h"
#include "lighting/shadows/shadows.h"
#include "src/core/engine.h"
#include "src/core/time.h"
#include "src/core/camera/free_camera.h"
#include "src/core/game_object/renderable.h"
#include "src/core/scene/serializer.h"
#include "src/util/file.h"

namespace will_engine
{
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

    terrainProperties = terrainComponent->getTerrainProperties();
    terrainSeed = terrainComponent->getSeed();
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
                ImGui::Columns(2, "StartupTimers");

                ImGui::Text("Operation");
                ImGui::NextColumn();
                ImGui::Text("Time (ms)");
                ImGui::NextColumn();
                ImGui::Separator();

                for (const auto& [name, timer] : engine->startupProfiler.getTimers()) {
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
        }
        ImGui::EndTabBar();
    }
    ImGui::End();

    if (ImGui::Begin("Renderer")) {
        if (ImGui::BeginTabBar("RendererTabs")) {
            if (ImGui::BeginTabItem("Shaders")) {
                ImGui::Text("Shaders");
                ImGui::SetNextItemWidth(75.0f);
                if (ImGui::Button("Hot-Reload Shaders")) {
                    engine->hotReloadShaders();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Pipelines")) {
                ImGui::Text("Deferred Debug");
                const char* deferredDebugOptions[]{"None", "Depth", "Velocity", "Albedo", "Normal", "PBR", "Shadows", "Cascade Level", "nDotL"};
                ImGui::Combo("Deferred Debug", &engine->deferredDebug, deferredDebugOptions, IM_ARRAYSIZE(deferredDebugOptions));
                ImGui::Text("Temporal Anti-Aliasing");
                ImGui::Checkbox("Enable TAA", &engine->bEnableTaa);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Debug View")) {
                ImGui::Text("Frustum Cull Debug Draw");
                ImGui::Checkbox("Enable Frustum Cull Debug Draw", &engine->bEnableDebugFrustumCullDraw);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Environment")) {
                ImGui::Text("Environment Map");
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
                if (ImGui::Combo("Environment", &currentIndex, getLabel, &data, static_cast<int>(indexNamePairs.size()))) {
                    engine->environmentMapIndex = indexNamePairs[currentIndex].first;
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Shadows")) {
                ImGui::Text("Cascaded Shadow Map");
                ImGui::DragInt("CSM PCF Level", &engine->csmPcf, 2, 1, 7);
                ImGui::DragFloat3("Main Light Direction", engine->mainLight.direction, 0.1);
                ImGui::InputFloat2("Cascade 1 Bias", shadows::CASCADE_BIAS[0]);
                ImGui::InputFloat2("Cascade 2 Bias", shadows::CASCADE_BIAS[1]);
                ImGui::InputFloat2("Cascade 3 Bias", shadows::CASCADE_BIAS[2]);
                ImGui::InputFloat2("Cascade 4 Bias", shadows::CASCADE_BIAS[3]);
                ImGui::SetNextItemWidth(100);
                static int32_t shadowMapDebug{0};
                ImGui::SliderInt("Shadow Map Level", &shadowMapDebug, 0, shadows::SHADOW_CASCADE_COUNT - 1);
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

            if (ImGui::BeginTabItem("Save Images")) {
                if (ImGui::Button("Save Draw Image")) {
                    if (file::getOrCreateDirectory(file::imagesSavePath)) {
                        const std::filesystem::path path = file::imagesSavePath / "drawImage.png";
                        vk_helpers::saveImageRGBA16SFLOAT(*engine->resourceManager, *engine->immediate, engine->drawImage,
                                                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, path.string().c_str());
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

                        vk_helpers::saveImageR32F(*engine->resourceManager, *engine->immediate, engine->depthImage,
                                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT, path.string().c_str(), depthNormalize);
                    }
                    else {
                        fmt::print(" Failed to find/create image save path directory");
                    }
                }

                if (ImGui::Button("Save Normals")) {
                    if (file::getOrCreateDirectory(file::imagesSavePath)) {
                        const std::filesystem::path path = file::imagesSavePath / "normalRT.png";
                        auto unpackFunc = [](const uint32_t packedColor) {
                            glm::vec4 pixel = glm::unpackSnorm4x8(packedColor);
                            pixel.r = pixel.r * 0.5f + 0.5f;
                            pixel.g = pixel.g * 0.5f + 0.5f;
                            pixel.b = pixel.b * 0.5f + 0.5f;
                            pixel.a = 1.0f;
                            return pixel;
                        };
                        vk_helpers::savePacked32Bit(*engine->resourceManager, *engine->immediate, engine->normalRenderTarget,
                                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, path.string().c_str(), unpackFunc);
                    }
                    else {
                        fmt::print(" Failed to save normal render target");
                    }
                }

                if (ImGui::Button("Save Albedo Render Target")) {
                    if (file::getOrCreateDirectory(file::imagesSavePath)) {
                        const std::filesystem::path path = file::imagesSavePath / "albedoRT.png";
                        auto unpackFunc = [](const uint32_t packedColor) {
                            glm::vec4 pixel = glm::unpackUnorm4x8(packedColor);
                            pixel.a = 1.0f;
                            return pixel;
                        };
                        vk_helpers::savePacked32Bit(*engine->resourceManager, *engine->immediate, engine->albedoRenderTarget,
                                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, path.string().c_str(), unpackFunc);
                    }
                    else {
                        fmt::print(" Failed to save albedo render target");
                    }
                }

                if (ImGui::Button("Save PBR Render Target")) {
                    if (file::getOrCreateDirectory(file::imagesSavePath)) {
                        std::filesystem::path path = file::imagesSavePath / "pbrRT.png";
                        auto unpackFunc = [](const uint32_t packedColor) {
                            glm::vec4 pixel = glm::unpackUnorm4x8(packedColor);
                            pixel.a = 1.0f;
                            return pixel;
                        };
                        vk_helpers::savePacked32Bit(*engine->resourceManager, *engine->immediate, engine->pbrRenderTarget,
                                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, path.string().c_str(), unpackFunc);
                    }
                    else {
                        fmt::print(" Failed to save pbr render target");
                    }
                }

                if (ImGui::Button("Save Post Process Resolve Target")) {
                    if (file::getOrCreateDirectory(file::imagesSavePath)) {
                        std::filesystem::path path = file::imagesSavePath / "postProcesResolve.png";
                        vk_helpers::saveImageRGBA16SFLOAT(*engine->resourceManager, *engine->immediate, engine->postProcessOutputBuffer,
                                                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, path.string().c_str());
                    }
                    else {
                        fmt::print(" Failed to find/create image save path directory");
                    }
                }
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
        for (Map* map : engine->activeMaps) {
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
                auto map = new Map(mapPath, *engine->resourceManager);
                engine->activeMaps.insert(map);
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
                static uint32_t selectedRenderObjectId = 0;

                ImGui::Text("Selected Render Object Details");

                float detailsHeight = 150.0f;
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
                                    GameObject* gob = selectedRenderObject->generateGameObject(std::string(objectName));
                                    selectedMap->addGameObject(gob);
                                    // todo: fix add full gameobject
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
                                if (auto container = dynamic_cast<IComponentContainer*>(selectedItem)) {
                                    if (ImGui::Button("Attach to selected item")) {
                                        if (!container->getMeshRenderer()) {
                                            auto newComponent = components::ComponentFactory::getInstance().createComponent(components::MeshRendererComponent::getStaticType(), "Mesh Renderer");
                                            container->addComponent(std::move(newComponent));
                                        }
                                        if (auto meshRenderer = container->getMeshRenderer()) {
                                            if (meshRenderer->hasMesh()) {
                                                meshRenderer->releaseMesh();
                                            }

                                            selectedRenderObject->generateMesh(meshRenderer, selectedMeshIndex);
                                        }
                                    }
                                }
                                else {
                                    if (ImGui::Button("Add to Scene")) {
                                        IHierarchical* gob = engine->createGameObject(selectedMap, objectName);

                                        if (auto _container = dynamic_cast<IComponentContainer*>(gob)) {
                                            auto newComponent = components::ComponentFactory::getInstance().createComponent(
                                                components::MeshRendererComponent::getStaticType(), "");
                                            _container->addComponent(std::move(newComponent));
                                            if (components::MeshRendererComponent* meshRenderer = _container->getMeshRenderer()) {
                                                selectedRenderObject->generateMesh(meshRenderer, selectedMeshIndex);
                                            }
                                            fmt::print("Added single mesh to scene\n");
                                        }
                                    }
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

                        willmodelPath = std::filesystem::current_path() / "assets" / "willmodels" / gltfPath.filename().string();
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

                if (ImGui::Button("Generate Texture Files")) {
                    generatedCount = 0;
                    if (!exists(texturesPath) || !is_directory(texturesPath)) {
                        fmt::print("Error: Invalid textures directory path: {}\n", texturesPath.string());
                        return;
                    }

                    std::vector<std::string> extensions = {".jpg", ".jpeg", ".png", ".tga", ".bmp"};

                    for (const auto& entry : std::filesystem::recursive_directory_iterator(texturesPath)) {
                        if (!entry.is_regular_file()) continue;

                        std::string extension = entry.path().extension().string();
                        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

                        if (std::find(extensions.begin(), extensions.end(), extension) != extensions.end()) {
                            const std::filesystem::path texturePath = entry.path();
                            const std::filesystem::path outputPath = texturePath.parent_path() /
                                                                     (texturePath.stem().string() + ".willtexture");

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
                    ImGui::Text("Failed to find any textures to generate.");
                    if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
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


    if (selectedItem) {
        if (IImguiRenderable* imguiRenderable = dynamic_cast<IImguiRenderable*>(selectedItem)) {
            imguiRenderable->selectedRenderImgui();
        }
    }

    ImGui::Render();
}

void ImguiWrapper::drawSceneGraph(Engine* engine)
{
    if (ImGui::BeginCombo("Select Map", selectedMap ? selectedMap->getName().data() : "None")) {
        for (Map* map : engine->activeMaps) {
            bool isSelected = (selectedMap == map);
            if (ImGui::Selectable(map->getName().data(), isSelected)) {
                selectMap(map);
            }

            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (selectedMap == nullptr) {
        if (!engine->activeMaps.empty()) {
            const auto firstMap = *engine->activeMaps.begin();
            selectMap(firstMap);
        }
        else {
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
                [[maybe_unused]] IHierarchical* gameObject = engine->createGameObject(selectedMap, fmt::format("New GameObject_{}", incrementId++));
            }
            ImGui::Separator();
            if (!selectedMap->getChildren().empty()) {
                for (IHierarchical* child : selectedMap->getChildren()) {
                    displayGameObject(engine, child, 0);
                }
            }
            else {
                ImGui::Text("Scene is empty");
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Terrain")) {
            ImGui::Checkbox("Draw Vertex Lines Only", &engine->bDrawTerrainLines);


            auto currentTerrainComponent = selectedMap->getComponent<components::TerrainComponent>();
            ImGui::BeginDisabled(!currentTerrainComponent);
            if (ImGui::Button("Save Terrain as HeightMap")) {
                std::vector<float> heightmapData = currentTerrainComponent->getHeightMapData();
                std::filesystem::path path = file::imagesSavePath / "TerrainHeightMap.png";
                vk_helpers::saveHeightmap(heightmapData, NOISE_MAP_DIMENSIONS, NOISE_MAP_DIMENSIONS, path);
            }

            ImGui::EndDisabled();
            ImGui::Separator();

            if (ImGui::CollapsingHeader("Noise Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::SliderFloat("Scale", &terrainProperties.scale, 1.0f, 200.0f);
                ImGui::SliderFloat("Persistence", &terrainProperties.persistence, 0.0f, 1.0f);
                ImGui::SliderFloat("Lacunarity", &terrainProperties.lacunarity, 1.0f, 5.0f);
                ImGui::SliderInt("Octaves", &terrainProperties.octaves, 1, 10);
                ImGui::DragFloat2("Offset", &terrainProperties.offset.x, 0.1f);
                ImGui::SliderFloat("Height Scale", &terrainProperties.heightScale, 1.0f, 200.0f);
            }

            ImGui::Separator();
            ImGui::InputScalar("Seed", ImGuiDataType_U32, &terrainSeed);
            ImGui::SameLine();

            static std::random_device rd{};
            static std::seed_seq ss{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
            static std::mt19937 gen(ss);
            static std::uniform_int_distribution<uint32_t> dist;
            if (ImGui::Button("Random Seed")) {
                terrainSeed = dist(gen);
            }

            ImGui::Separator();
            if (ImGui::Button("Generate Terrain", ImVec2(-1, 0))) {
                currentTerrainComponent->generateTerrain(terrainProperties, terrainSeed);
            }

            if (ImGui::Button("Destroy Terrain", ImVec2(-1, 0))) {
                currentTerrainComponent->destroyTerrain();
            }

            ImGui::EndTabItem();
        }


        ImGui::EndTabBar();
    }


    if (destroy) {
        selectedMap->destroy();
        selectMap(nullptr);
        selectedItem = nullptr;
    }
}

void ImguiWrapper::displayGameObject(Engine* engine, IHierarchical* obj, const int32_t depth)
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
        if (selectedItem == obj) {
            selectedItem = nullptr;
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

    if (obj == selectedItem) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.0f, 0.6f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.0f, 0.7f, 0.0f, 1.0f));
    }

    const bool isOpen = ImGui::TreeNodeEx("##TreeNode", flags, "%s", formattedName.c_str());

    if (obj == selectedItem) {
        ImGui::PopStyleColor(3);
    }

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        selectedItem = (obj == selectedItem) ? nullptr : obj;
    }
    ImGui::NextColumn();

    // Column 3: Control buttons
    if (const IHierarchical* parent = obj->getParent()) {
        constexpr float spacing = 5.0f;
        constexpr float buttonWidth = 60.0f;

        ImGui::BeginDisabled(parent == selectedMap);
        if (ImGui::Button("Undent", ImVec2(buttonWidth, 0))) {
            undent(obj);
        }
        ImGui::EndDisabled();

        ImGui::SameLine(0, spacing);
        const std::vector<IHierarchical*>& parentChildren = parent->getChildren();
        ImGui::BeginDisabled(parent != obj->getParent() || parentChildren[0] == obj);
        if (ImGui::Button("Indent", ImVec2(buttonWidth, 0))) {
            indent(obj);
        }
        ImGui::EndDisabled();

        ImGui::SameLine(0, spacing);
        ImGui::BeginDisabled(parent != obj->getParent() || parentChildren[0] == obj);
        if (ImGui::ArrowButton("##Up", ImGuiDir_Up)) {
            moveObject(obj, -1);
        }
        ImGui::EndDisabled();

        ImGui::SameLine(0, spacing);
        ImGui::BeginDisabled(parent != obj->getParent() || parentChildren[parentChildren.size() - 1] == obj);
        if (ImGui::ArrowButton("##Down", ImGuiDir_Down)) {
            moveObject(obj, 1);
        }
        ImGui::EndDisabled();
    }
    ImGui::NextColumn();

    ImGui::Columns(1);

    if (const auto imguiRenderable = dynamic_cast<IImguiRenderable*>(obj)) {
        imguiRenderable->renderImgui();
    }

    if (isOpen) {
        for (IHierarchical* child : obj->getChildren()) {
            displayGameObject(engine, child, depth + 1);
        }
        ImGui::TreePop();
    }

    ImGui::Unindent(static_cast<float>(depth * indentLength));
    ImGui::PopID();
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
    int32_t index = getIndexInVector(obj, currChildren);
    if (index == -1 || index == 0) { return; }

    obj->reparent(currChildren[index - 1]);
}

void ImguiWrapper::undent(IHierarchical* obj)
{
    // find first parent under scene root
    const IHierarchical* parent = obj->getParent();
    if (parent == nullptr) { return; }

    IHierarchical* parentParent = parent->getParent();
    if (parent->getParent() == nullptr) { return; }

    // get index of in scene Root
    std::vector<IHierarchical*>& parentParentChildren = parentParent->getChildren();
    const int32_t index = getIndexInVector(parent, parentParentChildren);
    if (index == -1) { return; }

    obj->reparent(parentParent);


    // already at end, no need to reorder.
    if (index == parentParentChildren.size() - 1) { return; }
    // move the child's position to index + 1 in the children vector
    std::rotate(parentParentChildren.begin() + index + 1, parentParentChildren.end() - 1, parentParentChildren.end());
}

void ImguiWrapper::moveObject(const IHierarchical* obj, int diff)
{
    assert(diff != 0);

    std::vector<IHierarchical*>& parentChildren = obj->getParent()->getChildren();
    const int32_t index = getIndexInVector(obj, parentChildren);
    // couldn't find. big error.
    if (index == -1) { return; }

    int32_t newIndex = index + diff;
    if (newIndex < 0 || newIndex >= parentChildren.size()) { return; }

    if (index < newIndex) {
        std::rotate(parentChildren.begin() + index, parentChildren.begin() + newIndex, parentChildren.begin() + index + 2);
    }
    else {
        std::rotate(parentChildren.begin() + newIndex, parentChildren.begin() + index, parentChildren.begin() + index + 1);
    }
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
}
