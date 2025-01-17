//
// Created by William on 8/11/2024.
//

#ifndef ENGINE_H
#define ENGINE_H

#include <deque>
#include <functional>


#include <vulkan/vulkan_core.h>


#include <VkBootstrap.h>

#include <SDL.h>
#include <glm/glm.hpp>
#include <src/renderer/renderer_constants.h>

#include "imgui_wrapper.h"
#include "scene.h"
#include "../renderer/vk_types.h"
#include "../renderer/vk_descriptor_buffer.h"
#include "../renderer/vk_helpers.h"
#include "src/renderer/environment/environment.h"
#include "src/renderer/lighting/directional_light.h"
#include "src/renderer/pipelines/post_processing/post_process_types.h"

class CascadedShadowMap;
class CascadedShadowMapDescriptorLayouts;
class ImguiWrapper;
class PlayerCharacter;
class Physics;
class ResourceManager;
class ImmediateSubmitter;
class DeferredResolvePipeline;
class DeferredMrtPipeline;
class RenderObjectDescriptorLayout;
class TaaPipeline;
class PostProcessPipeline;
class EnvironmentPipeline;
class FrustumCullingPipeline;
class FrustumCullingDescriptorLayouts;
class EnvironmentDescriptorLayouts;
class SceneDescriptorLayouts;

struct DeletionQueue
{
    std::deque<std::function<void()> > deleteQueue;

    void pushFunction(std::function<void()>&& function)
    {
        deleteQueue.push_back(function);
    }

    void flush()
    {
        // reverse iterate the deletion queue to execute all the functions
        for (auto& it : deleteQueue) {
            (it)();
        }

        deleteQueue.clear();
    }
};

struct FrameData
{
    VkCommandPool _commandPool;
    VkCommandBuffer _mainCommandBuffer;
    VkSemaphore _swapchainSemaphore, _renderSemaphore;
    VkFence _renderFence;

    // Frame Lifetime Deletion Queue
    DeletionQueue _deletionQueue;
};


class Engine
{
public:
    void init();

    void run();

    void draw();

    void update(float deltaTime) const;

    void updateSceneData(VkCommandBuffer cmd) const;

    /**
     * Cleans up vulkan resources when application has exited. Destroys resources in opposite order of initialization
     * \n Resources -> Command Pool (implicit destroy C. Buffers) -> Swapchain -> Surface -> Device -> Instance -> Window
     */
    void cleanup();

private: // Initialization
    void initRenderer();

    void initScene();

private: // Vulkan Boilerplate
    /**
     * The extents of the window. Used to initialize the swapchain image.
     */
    VkExtent2D windowExtent{1700, 900};
    /**
     * All graphics operation in this program operate with these extents and are scaled down with a blit into the window extents
     */
    const VkExtent2D renderExtent{1920, 1080};
    //const VkExtent2D renderExtent{3840, 2160};
    SDL_Window* window{nullptr};

    VulkanContext* context = nullptr;
    ImmediateSubmitter* immediate = nullptr;
    ResourceManager* resourceManager = nullptr;
    Physics* physics = nullptr;

    friend void ImguiWrapper::imguiInterface(Engine* engine);

    ImguiWrapper* imguiWrapper = nullptr;

    //DeletionQueue mainDeletionQueue;

private: // Rendering
    // Main
    int64_t frameNumber{0};
    FrameData frames[FRAME_OVERLAP]{};
    FrameData& getCurrentFrame() { return frames[frameNumber % FRAME_OVERLAP]; }
    bool bStopRendering{false};

    double frameTime{};
    double renderTime{};
    double gameTime{};
    double physicsTime{};

private: // Scene
    PlayerCharacter* player{nullptr};
    Scene scene{};

    RenderObject* sponza{nullptr};
    RenderObject* cube{nullptr};
    RenderObject* primitives{nullptr};
    GameObject* sponzaObject{nullptr};
    GameObject* cubeGameObject{nullptr};
    GameObject* cubeGameObject2{nullptr};
    GameObject* primitiveCubeGameObject{nullptr};
    std::vector<GameObject*> gameObjects{};
    std::vector<GameObject*> cameraDebugGameObjects{};
    std::vector<GameObject*> cascadeDebugGameObjects{};

    CascadedShadowMap* cascadedShadowMap{nullptr};

    Environment* environmentMap{nullptr};
    int32_t environmentMapIndex{0};

    DirectionalLight mainLight = {glm::normalize(glm::vec3(2.0f, -3.0f, 1.0f)), 1.0f, glm::vec3(0.0f)};

    int32_t deferredDebug{0};
    int32_t taaDebug{0};
    bool bDebugDisableShadows{false};
    int32_t shadowMapDebug{0};
    // if false shows bounds of ortho made from this persp
    bool bShowPerspectiveBounds{true};
    bool bShowOrthographicBounds{true};

private: // Scene Descriptors
    EnvironmentDescriptorLayouts* environmentDescriptorLayouts = nullptr;
    CascadedShadowMapDescriptorLayouts* shadowMapDescriptorLayouts = nullptr;
    SceneDescriptorLayouts* sceneDescriptorLayouts = nullptr;
    FrustumCullingDescriptorLayouts* frustumCullDescriptorLayouts = nullptr;
    RenderObjectDescriptorLayout* renderObjectDescriptorLayout = nullptr;

    DescriptorBufferUniform sceneDataDescriptorBuffer;
    AllocatedBuffer sceneDataBuffer;

    bool bEnableTaa{true};
    bool bEnableJitter{true};
    float taaBlend{0.1f};
    bool bEnableShadowMapDebug{false};
    bool bEnableFrustumCulling{true};

    PostProcessType postProcessFlags{PostProcessType::Sharpening | PostProcessType::Tonemapping};

private: // Pipelines
    // Frustum Culling
    FrustumCullingPipeline* frustumCullingPipeline{nullptr};

    EnvironmentPipeline* environmentPipeline{nullptr};

    // Deferred MRT
    DeferredMrtPipeline* deferredMrtPipeline{nullptr};

    // Deferred Resolve
    DeferredResolvePipeline* deferredResolvePipeline{nullptr};

    // TAA
    TaaPipeline* taaPipeline{nullptr};

    // PostProcess
    PostProcessPipeline* postProcessPipeline{nullptr};

private: // Swapchain
    VkSwapchainKHR swapchain{};
    VkFormat swapchainImageFormat{};
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkExtent2D swapchainExtent{};

    bool bResizeRequested{false};

    void createSwapchain(uint32_t width, uint32_t height);

    void resizeSwapchain();

private: // Render Targets
    const VkFormat drawImageFormat{VK_FORMAT_R16G16B16A16_SFLOAT};
    const VkFormat depthImageFormat{VK_FORMAT_D32_SFLOAT};
    const VkFormat velocityImageFormat{VK_FORMAT_R16G16_SFLOAT};
    const VkFormat normalImageFormat{VK_FORMAT_R16G16B16A16_SNORM}; //VK_FORMAT_R8G8B8A8_SNORM - 8888 is too inaccurate for normals
    const VkFormat albedoImageFormat{VK_FORMAT_R8G8B8A8_UNORM};
    const VkFormat pbrImageFormat{VK_FORMAT_R8G8B8A8_UNORM};

    void createDrawResources(uint32_t width, uint32_t height);

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

public: // Default Data
    static VkDescriptorSetLayout emptyDescriptorSetLayout;
};

#endif //ENGINE_H
