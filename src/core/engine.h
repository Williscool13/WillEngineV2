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
#include "camera/free_camera.h"
#include "environment.h"
#include "render_object.h"

constexpr unsigned int FRAME_OVERLAP = 2;

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

    void drawEnvironment(VkCommandBuffer cmd) const;

    void frustumCull(VkCommandBuffer cmd) const;

    void drawDeferredMrt(VkCommandBuffer cmd) const;

    void drawDeferredResolve(VkCommandBuffer cmd) const;

    void drawRender(VkCommandBuffer cmd) const;

    void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView);

    /**
     * Cleans up vulkan resources when application has exited. Destroys resources in opposite order of initialization
     * \n Resources -> Command Pool (implicit destroy C. Buffers) -> Swapchain -> Surface -> Device -> Instance -> Window
     */
    void cleanup();

private: // Initialization
    void initVulkan();

    void initSwapchain();

    void initCommands();

    void initSyncStructures();

    /**
    * Initialization of default textures and samplers for all models to use.
    * Used in cases where models don't have an albedo texture, they will fallback to he white texture (sometimes only vertex colors are assigned)
     */
    void initDefaultData();

    /**
     * Initializes all dear imgui vulkan/SDL2 integration. Mostly copied from imgui's samples
     */
    void initDearImgui();

    void initDescriptors();

    void initPipelines();

    void initFrustumCullingPipeline();

    void initDeferredMrtPipeline();

    void initDeferredResolvePipeline();

    void initRenderPipeline();

    void initEnvironmentPipeline();

    void initScene();

public:
    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function) const;

private: // Vulkan Boilerplate
    VkExtent2D windowExtent{1700, 900};
    SDL_Window* window{nullptr};

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
    int frameNumber{0};
    FrameData frames[FRAME_OVERLAP]{};
    FrameData& getCurrentFrame() { return frames[frameNumber % FRAME_OVERLAP]; };
    bool stopRendering{false};

    double frameTime{};
    double drawTime{};

    // Immediate Mode
    VkFence immFence{VK_NULL_HANDLE};
    VkCommandBuffer immCommandBuffer{VK_NULL_HANDLE};
    VkCommandPool immCommandPool{VK_NULL_HANDLE};

private: // Scene
    FreeCamera camera{};
    Scene scene{};

    RenderObject* testRenderObject{nullptr};
    GameObject* testGameObject1{nullptr};
    GameObject* testGameObject2{nullptr};
    GameObject* testGameObject3{nullptr};
    GameObject* testGameObject4{nullptr};
    GameObject* testGameObject5{nullptr};

    Environment* environment{nullptr};

private: // Scene Descriptors
    VkDescriptorSetLayout sceneDataDescriptorSetLayout{VK_NULL_HANDLE};
    DescriptorBufferUniform sceneDataDescriptorBuffer;
    /**
     * Descriptor for \code SceneData\endcode
     */
    AllocatedBuffer sceneDataBuffer;

    DescriptorBufferUniform spectateSceneDataDescriptorBuffer;
    AllocatedBuffer spectateSceneDataBuffer;
    glm::vec3 spectateCameraPosition{-0.4f, 0.4f, 0.f};
    glm::vec3 spectateCameraLookAt{0.5f, -0.3f, 0.f};

private: // Pipelines
    // Frustum Culling
    VkPipelineLayout frustumCullingPipelineLayout{VK_NULL_HANDLE};
    VkPipeline frustumCullingPipeline{VK_NULL_HANDLE};

    // deferred MRT
    VkPipelineLayout deferredMrtPipelineLayout{VK_NULL_HANDLE};
    VkPipeline deferredMrtPipeline{VK_NULL_HANDLE};

    // deferred resolve
    DescriptorBufferSampler deferredResolveDescriptorBuffer{};
    VkDescriptorSetLayout deferredResolveRenderTargetLayout{VK_NULL_HANDLE};
    VkPipelineLayout deferredResolvePipelineLayout{VK_NULL_HANDLE};
    VkPipeline deferredResolvePipeline{VK_NULL_HANDLE};

    // Render
    VkPipelineLayout renderPipelineLayout{VK_NULL_HANDLE};
    VkPipeline renderPipeline{VK_NULL_HANDLE};

    // Environment
    VkPipelineLayout environmentPipelineLayout{VK_NULL_HANDLE};
    VkPipeline environmentPipeline{VK_NULL_HANDLE};

private: // Swapchain
    VkSwapchainKHR swapchain{};
    VkFormat swapchainImageFormat{};
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkExtent2D swapchainExtent{};

    bool resizeRequested{false};

    void createSwapchain(uint32_t width, uint32_t height);

    void resizeSwapchain();

private: // Render Targets
    /**
     * 10.10.10 Normals - 2 unused
     */
    AllocatedImage normalRenderTarget{};
    /**
     * 10.10.10 RGB Albedo - 2 unused
     */
    AllocatedImage albedoRenderTarget{};
    /**
     * 8 Metallic, 8 Roughness, 8 emissive (unused), 8 Unused
     */
    AllocatedImage pbrRenderTarget{};

    void createRenderTargets(uint32_t width, uint32_t height);

private: // Draw Images
    // todo: In anticipation of deferred rendering, not implementing MSAA
    AllocatedImage drawImage{};
    AllocatedImage depthImage{};
    VkExtent2D drawExtent{};
    float renderScale{1.0f};
    float maxRenderScale{1.0f};

    void createDrawImages(uint32_t width, uint32_t height);

public: // Default Data
    static AllocatedImage whiteImage;
    static AllocatedImage errorCheckerboardImage;
    static VkSampler defaultSamplerLinear;
    static VkSampler defaultSamplerNearest;

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
