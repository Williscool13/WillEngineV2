//
// Created by William on 8/11/2024.
//

#ifndef ENGINE_H
#define ENGINE_H

#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>

#include "engine_types.h"
#include "camera/camera.h"
#include "camera/free_camera.h"
#include "scene/serializer.h"
#include "engine/core/profiler/profiler.h"
#include "engine/renderer/imgui_wrapper.h"
#include "engine/renderer/renderer_constants.h"
#include "engine/renderer/assets/asset_manager.h"
#include "engine/renderer/lighting/directional_light.h"
#include "engine/renderer/pipelines/post/post_process/post_process_pipeline_types.h"
#include "engine/renderer/pipelines/geometry/transparent_pipeline/transparent_pipeline.h"
#include "engine/renderer/pipelines/post/temporal_antialiasing/temporal_antialiasing_pipeline_types.h"
#include "engine/renderer/pipelines/shadows/cascaded_shadow_map/shadow_types.h"
#include "engine/renderer/pipelines/shadows/contact_shadow/contact_shadows_pipeline.h"
#include "engine/renderer/pipelines/shadows/ground_truth_ambient_occlusion/ambient_occlusion_types.h"
#include "engine/renderer/resources/descriptor_set_layout.h"
#include "engine/renderer/resources/pipeline.h"
#include "engine/renderer/resources/pipeline_layout.h"
#include "engine/renderer/resources/buffer.h"
#include "engine/renderer/resources/image_view.h"
#include "engine/renderer/resources/render_target.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_sampler.h"
#include "engine/renderer/resources/resources_fwd.h"
#include "events/event_dispatcher.h"

#if WILL_ENGINE_DEBUG_DRAW
namespace will_engine::renderer
{
class RenderContext;
class DebugRenderer;
class DebugHighlighter;
class DebugCompositePipeline;
}
#endif

namespace will_engine::renderer
{
class Environment;
class PostProcessPipeline;
class TemporalAntialiasingPipeline;
class GroundTruthAmbientOcclusionPipeline;
class CascadedShadowMap;
class DeferredResolvePipeline;
class DeferredMrtPipeline;
class TerrainPipeline;
class EnvironmentPipeline;
class VisibilityPassPipeline;
}

namespace will_engine::physics
{
class Physics;
}

namespace will_engine
{
namespace terrain
{
    class TerrainManager;
}



class Engine
{
public:
    static Engine* instance;

    static Engine* get() { return instance; }

public:
    void init();

    void initRenderer();

    bool generateDefaultMap();

    void initGame();

    void run();

    void updatePhysics(float deltaTime) const;

    void updateGame(float deltaTime);

    void updateRender(VkCommandBuffer cmd, float deltaTime, int32_t currentFrameOverlap, int32_t previousFrameOverlap);

    void updateDebug(float deltaTime);

    void render(float deltaTime);

    /**
     * Cleans up vulkan resources when application has exited. Destroys resources in opposite order of initialization
     * \n Resources -> Command Pool (implicit destroy C. Buffers) -> Swapchain -> Surface -> Device -> Instance -> Window
     */
    void cleanup();

#if WILL_ENGINE_DEBUG_DRAW
    void selectItem(IHierarchical* hierarchical);

    void deselectItem();
#endif

public:
    /**
     * Creates a gameobject as a child of the map parameter
     * @param map
     * @param name
     * @return
     */
    [[nodiscard]] static IHierarchical* createGameObject(game::Map* map, const std::string& name);

    void addToBeginQueue(IHierarchical* obj);

    void addToMapDeletionQueue(game::Map* obj);

    void addToDeletionQueue(std::unique_ptr<IHierarchical> obj);

public:
    renderer::AssetManager* getAssetManager() const { return assetManager; }
    renderer::ResourceManager* getResourceManager() const { return resourceManager; }

    void addToActiveTerrain(ITerrain* terrain);

    void removeFromActiveTerrain(ITerrain* terrain);

private:
    SDL_Window* window{nullptr};

    renderer::RenderContext* renderContext;
    renderer::VulkanContext* context{nullptr};
    renderer::ImmediateSubmitter* immediate{nullptr};
    renderer::ResourceManager* resourceManager{nullptr};
    renderer::AssetManager* assetManager{nullptr};
    physics::Physics* physics{nullptr};
#if WILL_ENGINE_DEBUG_DRAW
    renderer::DebugRenderer* debugRenderer{nullptr};
    renderer::DebugHighlighter* debugHighlighter{nullptr};
    renderer::DebugCompositePipeline* debugPipeline{nullptr};
    renderer::RenderTargetPtr debugTarget{};
#endif
    // Might be used in imgui which can be active outside of debug build
    IHierarchical* selectedItem{nullptr};

    renderer::Environment* environmentMap{nullptr};

    terrain::TerrainManager* terrainManager{nullptr};
    ImguiWrapper* imguiWrapper = nullptr;

    StartupProfiler startupProfiler{};
    Profiler profiler{};

private: // Rendering
    int32_t frameNumber{0};
    [[nodiscard]] int32_t getPreviousFrameOverlap() const { return glm::max(frameNumber - 1, 0) % FRAME_OVERLAP; }
    [[nodiscard]] int32_t getCurrentFrameOverlap() const { return frameNumber % FRAME_OVERLAP; }
    FrameData frames[FRAME_OVERLAP]{};
    FrameData& getCurrentFrame() { return frames[getCurrentFrameOverlap()]; }

    bool bStopRendering{false};
    bool bWindowChanged{false};

    void createDrawResources(VkExtent3D extents);

    EventDispatcher<int32_t> testDispatcher;

private: // Engine Settings
#if WILL_ENGINE_DEBUG
    EditorSettings editorSettings{};
#endif
    EngineSettings engineSettings{};
    renderer::ContactShadowSettings sssSettings{};
    renderer::GTAOSettings gtaoSettings{};
    renderer::CascadedShadowMapSettings csmSettings{};
    temporal_antialiasing_pipeline::TemporalAntialiasingSettings taaSettings{};

public:
#if WILL_ENGINE_DEBUG
    EditorSettings getEditorSettings() const { return editorSettings; }
    void setEditorSettings(const EditorSettings& settings) { editorSettings = settings; }
#endif
    EngineSettings getEngineSettings() const { return engineSettings; }
    void setEngineSettings(const EngineSettings& settings) { engineSettings = settings; }

    renderer::GTAOSettings getAoSettings() const { return gtaoSettings; }
    void setAoSettings(const renderer::GTAOSettings& settings) { gtaoSettings = settings; }

    renderer::ContactShadowSettings getSssSettings() const { return sssSettings; }
    void setSssSettings(const renderer::ContactShadowSettings& settings) { sssSettings = settings; }

    renderer::CascadedShadowMapSettings getCsmSettings() const { return csmSettings; }

    void setCsmSettings(const renderer::CascadedShadowMapSettings& settings);

    temporal_antialiasing_pipeline::TemporalAntialiasingSettings getTaaSettings() const { return taaSettings; }
    void setTaaSettings(const temporal_antialiasing_pipeline::TemporalAntialiasingSettings& settings) { taaSettings = settings; }

private: // Debug
    int32_t deferredDebug{0};
    bool bEnablePhysics{true};
    bool bDrawTransparents{true};
    bool bEnableShadows{true};
    bool bEnableContactShadows{true};
    bool bDrawDebugRendering{true};
    bool bDebugPhysics{true};
    bool bFreezeVisibilitySceneData{false};

    void hotReloadShaders() const;

public:
    Camera* getCamera() const { return fallbackCamera; }
    DirectionalLight getMainLight() const { return mainLight; }
    void setMainLight(const DirectionalLight& newLight) { mainLight = newLight; }

    int32_t getCurrentEnvironmentMapIndex() const { return environmentMapIndex; }
    void setCurrentEnvironmentMapIndex(const int32_t index) { environmentMapIndex = index; }

private:
    EventDispatcher<renderer::ResolutionChangedEvent>::Handle resolutionChangedHandle;
    EventDispatcher<renderer::ResolutionChangedEvent>::Handle postResolutionChangedHandle;

    void handleResize(const renderer::ResolutionChangedEvent& event);
    void setupDescriptorBuffers() const;

private: // Scene Data
    renderer::DescriptorBufferUniformPtr sceneDataDescriptorBuffer;
    std::array<renderer::BufferPtr, FRAME_OVERLAP> sceneDataBuffers;
    renderer::BufferPtr debugSceneDataBuffer{nullptr};
    SceneData snapshotSceneData{};

    /**
     * Should always exist and be used if no other camera is in the scene (todo: camera system)
     */
    FreeCamera* fallbackCamera{nullptr};
    DirectionalLight mainLight{glm::normalize(glm::vec3(-0.8f, -0.6f, -0.6f)), 1.5f, glm::vec3(1.0f)};
    int32_t environmentMapIndex{1};

    renderer::PostProcessType postProcessData{
        renderer::PostProcessType::Tonemapping | renderer::PostProcessType::Sharpening
    };
    std::vector<std::unique_ptr<game::Map> > activeMaps;
    std::unordered_set<ITerrain*> activeTerrains;


    std::vector<IHierarchical*> hierarchalBeginQueue{};
    std::vector<std::unique_ptr<IHierarchical> > hierarchicalDeletionQueue{};
    std::vector<std::unique_ptr<game::Map> > mapDeletionQueue{};

private: // Pipelines
    renderer::VisibilityPassPipeline* visibilityPassPipeline{nullptr};

    renderer::EnvironmentPipeline* environmentPipeline{nullptr};
    renderer::TerrainPipeline* terrainPipeline{nullptr};
    renderer::DeferredMrtPipeline* deferredMrtPipeline{nullptr};
    renderer::DeferredResolvePipeline* deferredResolvePipeline{nullptr};
    renderer::TransparentPipeline* transparentPipeline{nullptr};

    renderer::CascadedShadowMap* cascadedShadowMap{nullptr};
    renderer::ContactShadowsPipeline* contactShadowsPipeline{nullptr};
    renderer::GroundTruthAmbientOcclusionPipeline* ambientOcclusionPipeline{nullptr};

    renderer::TemporalAntialiasingPipeline* temporalAntialiasingPipeline{nullptr};

    renderer::PostProcessPipeline* postProcessPipeline{nullptr};

private: // Draw Resources

    renderer::RenderTargetPtr drawImage{nullptr};
    renderer::RenderTargetPtr depthStencilImage{nullptr};
    renderer::ImageViewPtr depthImageView{nullptr};
    renderer::ImageViewPtr stencilImageView{nullptr};

    /**
     * 10,10,10 View Normals - 2 unused
     */
    renderer::RenderTargetPtr normalRenderTarget{nullptr};
    /**
     * 16,16,16 RGB Albedo (HDR) - 16 indicates if the image should be shaded
     */
    renderer::RenderTargetPtr albedoRenderTarget{nullptr};
    /**
     * 8 Metallic, 8 Roughness, 8 Unused, 8 Is Transparent
     */
    renderer::RenderTargetPtr pbrRenderTarget{nullptr};
    /**
     * 16 X and 16 Y
     */
    renderer::RenderTargetPtr velocityRenderTarget{nullptr};
    /**
    * The results of the TAA pass will be outputted into this buffer
    */
    renderer::RenderTargetPtr taaResolveTarget{nullptr};

    /**
     * A copy of the previous TAA Resolve Buffer
     */
    renderer::RenderTargetPtr historyBuffer{nullptr};

    renderer::RenderTargetPtr finalImageBuffer{nullptr};

private: // Swapchain
    VkSwapchainKHR swapchain{};
    VkFormat swapchainImageFormat{};
    std::vector<VkImage> swapchainImages{};
    std::vector<VkImageView> swapchainImageViews{};
    VkExtent3D swapchainExtent{};

    void createSwapchain(uint32_t width, uint32_t height);

public:
    game::Map* createMap(const std::filesystem::path& path);

    friend class ImguiWrapper;
};
}


#endif //ENGINE_H
