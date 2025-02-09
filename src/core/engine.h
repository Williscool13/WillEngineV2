//
// Created by William on 8/11/2024.
//

#ifndef ENGINE_H
#define ENGINE_H
#include <SDL_video.h>
#include <unordered_map>
#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>

#include "engine_types.h"
#include "scene/scene_serializer.h"
#include "src/renderer/imgui_wrapper.h"
#include "src/renderer/renderer_constants.h"
#include "src/renderer/vk_types.h"
#include "src/renderer/descriptor_buffer/descriptor_buffer_uniform.h"
#include "src/renderer/lighting/directional_light.h"


class ResourceManager;
class ImmediateSubmitter;
class VulkanContext;

namespace will_engine
{
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

namespace visibility_pass
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
    void init();

    void initRenderer();

    void initGame();

    void run();

    void update(float deltaTime);

    void updateGame(float deltaTime) const;

    void updateRender(float deltaTime, int32_t currentFrameOverlap, int32_t previousFrameOverlap) const;

    void draw(float deltaTime);

    /**
     * Cleans up vulkan resources when application has exited. Destroys resources in opposite order of initialization
     * \n Resources -> Command Pool (implicit destroy C. Buffers) -> Swapchain -> Surface -> Device -> Instance -> Window
     */
    void cleanup();

private:
    VkExtent2D windowExtent{1700, 900};
    SDL_Window* window{nullptr};

    VulkanContext* context{nullptr};
    ImmediateSubmitter* immediate = nullptr;
    ResourceManager* resourceManager = nullptr;
    identifier::IdentifierManager* identifierManager = nullptr;
    physics::Physics* physics = nullptr;
    environment::Environment* environmentMap{nullptr};
    cascaded_shadows::CascadedShadowMap* cascadedShadowMap{nullptr};
    ImguiWrapper* imguiWrapper = nullptr;

    EngineStats stats{};

private: // Rendering
    int32_t frameNumber{0};
    int32_t getPreviousFrameOverlap() const { return glm::max(frameNumber - 1, 0) % FRAME_OVERLAP; }
    int32_t getCurrentFrameOverlap() const { return frameNumber % FRAME_OVERLAP; }
    FrameData frames[FRAME_OVERLAP]{};
    FrameData& getCurrentFrame() { return frames[getCurrentFrameOverlap()]; }

    bool bStopRendering{false};
    bool bResizeRequested{false};

    void createDrawResources();

private: // Debug
    bool bEnableTaa{true};
    bool bEnableDebugFrustumCullDraw{false};
    int32_t deferredDebug{0};

    void hotReloadShaders() const;

private: // Scene Data
    DescriptorBufferUniform sceneDataDescriptorBuffer;
    AllocatedBuffer sceneDataBuffers[FRAME_OVERLAP]{};
    AllocatedBuffer debugSceneDataBuffer{};

    FreeCamera* camera{nullptr};
    DirectionalLight mainLight{glm::normalize(glm::vec3(-0.8f, -0.6f, -0.6f)), 1.0f, glm::vec3(0.0f)};
    int32_t environmentMapIndex{0};

    Scene* scene{nullptr};

    std::unordered_map<uint32_t, RenderObject*> newRenderObjects;
    std::unordered_map<uint32_t, RenderObjectInfo> renderObjectInfos;
    std::vector<RenderObject*> renderObjects{};
    RenderObject* cube{nullptr};
    RenderObject* primitives{nullptr};
    RenderObject* sponza{nullptr};
    RenderObject* mySphere{nullptr};
    //RenderObject* checkeredFloor{nullptr};

    std::vector<GameObject*> gameObjects{};

private: // Pipelines
    visibility_pass::VisibilityPassPipeline* frustumCullPipeline{nullptr};
    environment_pipeline::EnvironmentPipeline* environmentPipeline{nullptr};
    deferred_mrt::DeferredMrtPipeline* deferredMrtPipeline{nullptr};
    deferred_resolve::DeferredResolvePipeline* deferredResolvePipeline{nullptr};
    temporal_antialiasing_pipeline::TemporalAntialiasingPipeline* temporalAntialiasingPipeline{nullptr};
    post_process_pipeline::PostProcessPipeline* postProcessPipeline{nullptr};

private: // Draw Resources
    AllocatedImage drawImage{};
    AllocatedImage depthImage{};

    /**
     * 8.8.8 Normals - 8 unused
     */
    AllocatedImage normalRenderTarget{};
    /**
     * 8.8.8 RGB Albedo - 8 unused
     */
    AllocatedImage albedoRenderTarget{};
    /**
     * 8 Metallic, 8 Roughness, 8 emission (unused), 8 Unused
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
    AllocatedImage postProcessOutputBuffer{};

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
};
}


#endif //ENGINE_H
