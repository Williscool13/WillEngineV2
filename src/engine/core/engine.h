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
#include "engine/renderer/vk_types.h"
#include "engine/renderer/assets/asset_manager.h"
#include "engine/renderer/descriptor_buffer/descriptor_buffer_uniform.h"
#include "engine/renderer/lighting/directional_light.h"
#include "engine/renderer/pipelines/post/post_process/post_process_pipeline_types.h"
#include "engine/renderer/pipelines/geometry/transparent_pipeline/transparent_pipeline.h"
#include "engine/renderer/pipelines/post/temporal_antialiasing/temporal_antialiasing_pipeline_types.h"
#include "engine/renderer/pipelines/shadows/cascaded_shadow_map/shadow_types.h"
#include "engine/renderer/pipelines/shadows/contact_shadow/contact_shadows_pipeline.h"
#include "engine/renderer/pipelines/shadows/ground_truth_ambient_occlusion/ambient_occlusion_types.h"


namespace will_engine::debug_renderer
{
class DebugRenderer;
}

class ResourceManager;
class ImmediateSubmitter;
class VulkanContext;

namespace will_engine
{
namespace debug_highlight_pipeline
{
    class DebugHighlighter;
}

namespace debug_pipeline
{
    class DebugCompositePipeline;
}

namespace ambient_occlusion
{
    class GroundTruthAmbientOcclusionPipeline;
}

namespace terrain
{
    class TerrainChunk;
    class TerrainPipeline;
    class TerrainManager;
}

namespace identifier
{
    class IdentifierManager;
}

class Scene;

namespace post_process_pipeline
{
    class PostProcessPipeline;
}

namespace deferred_resolve
{
    class DeferredResolvePipeline;
}

namespace deferred_mrt
{
    class DeferredMrtPipeline;
}

namespace visibility_pass_pipeline
{
    class VisibilityPassPipeline;
}

class FreeCamera;
class ImguiWrapper;

namespace cascaded_shadows
{
    class CascadedShadowMap;
}

namespace environment
{
    class Environment;
}

namespace temporal_antialiasing_pipeline
{
    class TemporalAntialiasingPipeline;
}

class RenderObject;
class GameObject;

namespace environment_pipeline
{
    class EnvironmentPipeline;
}

namespace physics
{
    class Physics;
}

class Engine
{
public:
    static Engine* instance;

    static Engine* get() { return instance; }

public:
    void init();

    void initRenderer();

    void initGame();

    void run();

    void updatePhysics(float deltaTime) const;

    void updateGame(float deltaTime);

    void updateRender(VkCommandBuffer cmd, float deltaTime, int32_t currentFrameOverlap, int32_t previousFrameOverlap) const;

    void render(float deltaTime);

    /**
     * Cleans up vulkan resources when application has exited. Destroys resources in opposite order of initialization
     * \n Resources -> Command Pool (implicit destroy C. Buffers) -> Swapchain -> Surface -> Device -> Instance -> Window
     */
    void cleanup();

public:
    /**
     * Creates a gameobject as a child of the map parameter
     * @param map
     * @param name
     * @return
     */
    [[nodiscard]] static IHierarchical* createGameObject(Map* map, const std::string& name);

    void addToBeginQueue(IHierarchical* obj);

    void addToDeletionQueue(IHierarchical* obj);

public:
    AssetManager* getAssetManager() const { return assetManager; }
    ResourceManager* getResourceManager() const { return resourceManager; }

    void addToActiveTerrain(ITerrain* terrain);

    void removeFromActiveTerrain(ITerrain* terrain);

private:
    VkExtent2D windowExtent{1700, 900};
    SDL_Window* window{nullptr};

    VulkanContext* context{nullptr};
    ImmediateSubmitter* immediate{nullptr};
    ResourceManager* resourceManager{nullptr};
    AssetManager* assetManager{nullptr};
    physics::Physics* physics{nullptr};
#if WILL_ENGINE_DEBUG
    debug_renderer::DebugRenderer* debugRenderer{nullptr};
    debug_highlight_pipeline::DebugHighlighter* debugHighlighter{nullptr};
    debug_pipeline::DebugCompositePipeline* debugPipeline{nullptr};
#endif
    // Might be used in imgui which can be active outside of debug build
    IHierarchical* selectedItem{nullptr};

    environment::Environment* environmentMap{nullptr};

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
    bool bResizeRequested{false};

    void createDrawResources();

private: // Engine Settings
    EngineSettings engineSettings{};
    contact_shadows_pipeline::ContactShadowSettings sssSettings{};
    ambient_occlusion::GTAOSettings gtaoSettings{};
    cascaded_shadows::CascadedShadowMapSettings csmSettings{};
    temporal_antialiasing_pipeline::TemporalAntialiasingSettings taaSettings{};

public:
    EngineSettings getEngineSettings() const { return engineSettings; }
    void setEngineSettings(const EngineSettings& settings) { engineSettings = settings; }

    ambient_occlusion::GTAOSettings getAoSettings() const { return gtaoSettings; }
    void setAoSettings(const ambient_occlusion::GTAOSettings& settings) { gtaoSettings = settings; }

    contact_shadows_pipeline::ContactShadowSettings getSssSettings() const { return sssSettings; }
    void setSssSettings(const contact_shadows_pipeline::ContactShadowSettings& settings) { sssSettings = settings; }

    cascaded_shadows::CascadedShadowMapSettings getCsmSettings() const { return csmSettings; }
    void setCsmSettings(const cascaded_shadows::CascadedShadowMapSettings& settings);

    temporal_antialiasing_pipeline::TemporalAntialiasingSettings getTaaSettings() const { return taaSettings; }
    void setTaaSettings(const temporal_antialiasing_pipeline::TemporalAntialiasingSettings& settings) { taaSettings = settings; }

private: // Debug
    bool bEnableDebugFrustumCullDraw{false};
    int32_t deferredDebug{0};
    bool bDrawTerrainLines{false};
    bool bEnablePhysics{true};
    bool bDrawTransparents{true};
    bool bEnableShadows{true};
    bool bEnableContactShadows{true};
    bool bDrawDebugRendering{true};
    bool bDebugPhysics{true};

    void hotReloadShaders() const;

public:
    Camera* getCamera() const { return camera; }
    DirectionalLight getMainLight() const { return mainLight; }
    void setMainLight(const DirectionalLight& newLight) { mainLight = newLight; }

    int32_t getCurrentEnvironmentMapIndex() const { return environmentMapIndex; }
    void setCurrentEnvironmentMapIndex(const int32_t index) { environmentMapIndex = index; }

private: // Scene Data
    DescriptorBufferUniform sceneDataDescriptorBuffer;
    AllocatedBuffer sceneDataBuffers[FRAME_OVERLAP]{};
    AllocatedBuffer debugSceneDataBuffer{};

    FreeCamera* camera{nullptr};
    DirectionalLight mainLight{glm::normalize(glm::vec3(-0.8f, -0.6f, -0.6f)), 1.5f, glm::vec3(1.0f)};
    int32_t environmentMapIndex{1};

    post_process_pipeline::PostProcessType postProcessData{
        post_process_pipeline::PostProcessType::Tonemapping | post_process_pipeline::PostProcessType::Sharpening
    };

    std::unordered_set<Map*> activeMaps;
    std::unordered_set<ITerrain*> activeTerrains;


    std::vector<IHierarchical*> hierarchalBeginQueue{};
    std::vector<IHierarchical*> hierarchicalDeletionQueue{};

private: // Pipelines
    visibility_pass_pipeline::VisibilityPassPipeline* visibilityPassPipeline{nullptr};

    environment_pipeline::EnvironmentPipeline* environmentPipeline{nullptr};
    terrain::TerrainPipeline* terrainPipeline{nullptr};
    deferred_mrt::DeferredMrtPipeline* deferredMrtPipeline{nullptr};
    deferred_resolve::DeferredResolvePipeline* deferredResolvePipeline{nullptr};
    transparent_pipeline::TransparentPipeline* transparentPipeline{nullptr};

    cascaded_shadows::CascadedShadowMap* cascadedShadowMap{nullptr};
    contact_shadows_pipeline::ContactShadowsPipeline* contactShadowsPipeline{nullptr};
    ambient_occlusion::GroundTruthAmbientOcclusionPipeline* ambientOcclusionPipeline{nullptr};

    temporal_antialiasing_pipeline::TemporalAntialiasingPipeline* temporalAntialiasingPipeline{nullptr};

    post_process_pipeline::PostProcessPipeline* postProcessPipeline{nullptr};

private: // Draw Resources
    AllocatedImage drawImage{};
    /**
     * Image view in this depth image is VK_IMAGE_ASPECT_DEPTH_BIT
     */
    AllocatedImage depthStencilImage{};
    VkImageView depthImageView{VK_NULL_HANDLE};
    VkImageView stencilImageView{VK_NULL_HANDLE};

    /**
     * 8.8.8 View Normals - 8 unused
     */
    AllocatedImage normalRenderTarget{};
    /**
     * 8.8.8 RGB Albedo - 8 unused
     */
    AllocatedImage albedoRenderTarget{};
    /**
     * 8 Metallic, 8 Roughness, 8 emission (unused), 8 Is Transparent
     */
    AllocatedImage pbrRenderTarget{};
    /**
     * 16 X and 16 Y
     */
    AllocatedImage velocityRenderTarget{};
    /**
    * The results of the TAA pass will be outputted into this buffer
    */
    AllocatedImage taaResolveTarget{};

    /**
     * A copy of the previous TAA Resolve Buffer
     */
    AllocatedImage historyBuffer{};

#if WILL_ENGINE_DEBUG
    AllocatedImage debugTarget{};
#endif

    AllocatedImage finalImageBuffer{};

private: // Swapchain
    VkSwapchainKHR swapchain{};
    VkFormat swapchainImageFormat{};
    std::vector<VkImage> swapchainImages{};
    std::vector<VkImageView> swapchainImageViews{};
    VkExtent2D swapchainExtent{};

    void createSwapchain(uint32_t width, uint32_t height);

    void resizeSwapchain();

public:
    friend void ImguiWrapper::imguiInterface(Engine* engine);

    friend class ImguiWrapper;
};
}


#endif //ENGINE_H
