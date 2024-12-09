//
// Created by William on 8/11/2024.
//

#ifndef ENGINE_H
#define ENGINE_H

#include <chrono>
#include <deque>
#include <functional>
#include <array>
#include <thread>


#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vk_enum_string_helper.h>
#include <volk.h>
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>

#include <SDL.h>
#include <SDL_vulkan.h>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include <glm/glm.hpp>

#include "scene.h"
#include "../renderer/vk_types.h"
#include "../renderer/vk_descriptors.h"
#include "../renderer/vk_descriptor_buffer.h"
#include "../renderer/vk_helpers.h"
#include "../util/render_utils.h"
#include "camera/free_camera.h"
#include "src/renderer/vulkan_context.h"
#include "src/renderer/environment/environment.h"

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
constexpr unsigned int FRAME_OVERLAP = 2;
constexpr char ENGINE_NAME[] = "Will Engine";

struct DeletionQueue
{
    std::deque<std::function<void()> > deletors;

    void pushFunction(std::function<void()>&& function)
    {
        deletors.push_back(function);
    }

    void flush()
    {
        // reverse iterate the deletion queue to execute all the functions
        for (auto& it : deletors) {
            (it)();
        }

        deletors.clear();
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

    void updateSceneData() const;

    void updateSceneObjects() const;

    void DEBUG_drawSpectate(VkCommandBuffer cmd, const std::vector<RenderObject*>& renderObjects) const;

    void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView) const;

    /**
     * Cleans up vulkan resources when application has exited. Destroys resources in opposite order of initialization
     * \n Resources -> Command Pool (implicit destroy C. Buffers) -> Swapchain -> Surface -> Device -> Instance -> Window
     */
    void cleanup();

private: // Initialization
    /**
    * Initialization of default textures and samplers for all models to use.
    * Used in cases where models don't have an albedo texture, they will fallback to he white texture (sometimes only vertex colors are assigned)
     */
    void initDefaultData() const;

    /**
     * Initializes all dear imgui vulkan/SDL2 integration. Mostly copied from imgui's samples
     */
    void initDearImgui();

    void initDescriptors();

    void initScene();

public:
    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function) const;

private: // Vulkan Boilerplate
    /**
     * The extents of the window. Used to initialize the swapchain image.
     */
    VkExtent2D windowExtent{1700, 900};
    SDL_Window* window{nullptr};

    VulkanContext* context = nullptr;

    VkInstance instance{};
    VkSurfaceKHR surface{};
    VkPhysicalDevice physicalDevice{};
    VkDevice device{};
    VkQueue graphicsQueue{};
    uint32_t graphicsQueueFamily{};
    VmaAllocator allocator{};
    VkDebugUtilsMessengerEXT debug_messenger{};

    DeletionQueue mainDeletionQueue;

public:
    VkInstance getInstance() const { return instance; }

    VkDevice getDevice() const { return device; }

    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }

    VmaAllocator getAllocator() const { return allocator; }

private: // Rendering
    // Main
    int64_t frameNumber{0};
    FrameData frames[FRAME_OVERLAP]{};
    int32_t getCurrentFrameOverlap() const { return static_cast<int32_t>(frameNumber % FRAME_OVERLAP); }
    FrameData& getCurrentFrame() { return frames[frameNumber % FRAME_OVERLAP]; }
    bool bStopRendering{false};

    double frameTime{};
    double drawTime{};
    double renderTime{};

    // Immediate Mode
    VkFence immFence{VK_NULL_HANDLE};
    VkCommandBuffer immCommandBuffer{VK_NULL_HANDLE};
    VkCommandPool immCommandPool{VK_NULL_HANDLE};

private: // Scene
    FreeCamera camera{75.0f, 1920.0f / 1080.0f, 1000, 0.01};
    Scene scene{};

    RenderObject* testRenderObject{nullptr};
    RenderObject* cube{nullptr};
    RenderObject* primitives{nullptr};
    GameObject* cubeGameObject{nullptr};
    GameObject* cubeGameObject2{nullptr};
    GameObject* testGameObject1{nullptr};
    GameObject* testGameObject2{nullptr};
    GameObject* testGameObject3{nullptr};
    GameObject* testGameObject4{nullptr};
    GameObject* testGameObject5{nullptr};
    GameObject* primitiveObject{nullptr};


    Environment* environmentMap{nullptr};
    int32_t environmentMapindex{7};

    int32_t deferredDebug{0};
    int32_t taaDebug{0};

private: // Scene Descriptors
    EnvironmentDescriptorLayouts* environmentDescriptorLayouts = nullptr;
    SceneDescriptorLayouts* sceneDescriptorLayouts = nullptr;
    FrustumCullingDescriptorLayouts* frustumCullDescriptorLayouts = nullptr;
    RenderObjectDescriptorLayout* renderObjectDescriptorLayout = nullptr;

    DescriptorBufferUniform sceneDataDescriptorBuffer;
    AllocatedBuffer sceneDataBuffer;

    bool bSpectateCameraActive{false};
    DescriptorBufferUniform spectateSceneDataDescriptorBuffer;
    AllocatedBuffer spectateSceneDataBuffer;
    glm::vec3 spectateCameraPosition{-9.0f, 1.0f, 0.f};
    glm::vec3 spectateCameraLookAt{0.5f, 1.8f, 0.f};

    bool bEnableTaa{true};
    bool bEnableJitter{true};
    float taaMinBlend{0.03f};
    float taaMaxBlend{0.95f};
    float taaVelocityWeight{200.0f};

    bool bEnablePostProcess{true};

private: // Pipelines
    // Frustum Culling
    FrustumCullingPipeline* frustumCullingPipeline{nullptr};

    EnvironmentPipeline* environmentPipeline{nullptr};

    // deferred MRT
    DeferredMrtPipeline* deferredMrtPipeline{nullptr};

    // deferred resolve
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
    /**
     * All graphics operation in this program operate with these extent.
     */
    //const VkExtent2D renderExtent{1920, 1080};
    const VkExtent2D renderExtent{3840, 2160};
    const VkFormat drawImageFormat{VK_FORMAT_R16G16B16A16_SFLOAT};
    const VkFormat depthImageFormat{VK_FORMAT_D32_SFLOAT};
    const VkFormat velocityImageFormat{VK_FORMAT_R16G16_SFLOAT};
    const VkFormat normalImageFormat{VK_FORMAT_R8G8B8A8_SNORM};
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
     * 8 Metallic, 8 Roughness, 8 emissive (unused), 8 Unused
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
    static AllocatedImage whiteImage;
    static AllocatedImage errorCheckerboardImage;
    static VkSampler defaultSamplerLinear;
    static VkSampler defaultSamplerNearest;
    static VkDescriptorSetLayout emptyDescriptorSetLayout;

private: // DearImgui
    VkDescriptorPool imguiPool{VK_NULL_HANDLE};

public: // Buffers
    AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) const;

    AllocatedBuffer createStagingBuffer(size_t allocSize) const;

    AllocatedBuffer createReceivingBuffer(size_t allocSize) const;

    /**
     *
     * @param src
     * @param dst destination AllocatedBuffer. Must have already been created.
     * @param size
     */
    void copyBuffer(const AllocatedBuffer& src, const AllocatedBuffer& dst, VkDeviceSize size) const;

    VkDeviceAddress getBufferAddress(const AllocatedBuffer& buffer) const;

    void destroyBuffer(AllocatedBuffer& buffer) const;

public: // Images
    AllocatedImage createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false) const;

    AllocatedImage createImage(const void* data, size_t dataSize, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                               bool mipmapped = false) const;

    AllocatedImage createCubemap(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false) const;

    void destroyImage(const AllocatedImage& img) const;
};

#endif //ENGINE_H
