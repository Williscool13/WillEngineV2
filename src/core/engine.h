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

#include "game_object.h"
#include "../renderer/vk_types.h"
#include "../renderer/vk_descriptors.h"
#include "../renderer/vk_descriptor_buffer.h"
#include "../renderer/vk_helpers.h"
#include "camera/free_camera.h"

constexpr unsigned int FRAME_OVERLAP = 2;


struct DeletionQueue {
    std::deque<std::function<void()> > deletors;

    void pushFunction(std::function<void()>&& function)
    {
        deletors.push_back(function);
    }

    void flush()
    {
        // reverse iterate the deletion queue to execute all the functions
        for (auto& it: deletors) {
            (it)();
        }

        deletors.clear();
    }
};

struct FrameData {
    VkCommandPool _commandPool;
    VkCommandBuffer _mainCommandBuffer;
    VkSemaphore _swapchainSemaphore, _renderSemaphore;
    VkFence _renderFence;

    // Frame Lifetime Deletion Queue
    DeletionQueue _deletionQueue;
};


class Engine {
public:
    void init();

    void run();

    void draw();

    void drawCompute(VkCommandBuffer cmd);

    void drawRender(VkCommandBuffer cmd);

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

    void initDefaultData();

    void initDearImgui();

    void initPipelines();

    void initComputePipelines();

    void initRenderPipelines();

    void initScene();

    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function) const;

private: // Vulkan Boilerplate
    VkExtent2D windowExtent{1700, 900};
    SDL_Window *window{nullptr};

    VkInstance instance{};
    VkSurfaceKHR surface{};
    VkPhysicalDevice physicalDevice{};
    VkDevice device{};
    VkQueue graphicsQueue{};
    uint32_t graphicsQueueFamily{};
    VmaAllocator allocator{};
    VkDebugUtilsMessengerEXT debug_messenger{};

    DeletionQueue mainDeletionQueue;

private: // Rendering
    // Main
    int frameNumber{0};
    FrameData frames[FRAME_OVERLAP]{};
    FrameData &getCurrentFrame() { return frames[frameNumber % FRAME_OVERLAP]; };
    bool stopRendering{false};

    float frameTime{};
    float drawTime{};

    // Immediate Mode
    VkFence immFence{VK_NULL_HANDLE};
    VkCommandBuffer immCommandBuffer{VK_NULL_HANDLE};
    VkCommandPool immCommandPool{VK_NULL_HANDLE};

private: // Scene
    FreeCamera camera{};

    GameObject* tempObjectOne = nullptr;
    GameObject* tempObjectTwo = nullptr;

private: // Pipelines
    VkDescriptorSetLayout computeImageDescriptorSetLayout;
    DescriptorBufferSampler computeImageDescriptorBuffer;
    VkPipelineLayout backgroundEffectPipelineLayout;

    VkPipeline computePipeline;

    VkDescriptorSetLayout renderImageDescriptorSetLayout;
    VkDescriptorSetLayout renderUniformDescriptorSetLayout;
    DescriptorBufferSampler renderImageDescriptorBuffer;
    DescriptorBufferUniform renderUniformDescriptorBuffer;
    VkPipelineLayout renderPipelineLayout;

    VkPipeline renderPipeline;

private: // Swapchain
    VkSwapchainKHR swapchain{};
    VkFormat swapchainImageFormat{};
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkExtent2D swapchainExtent{};

    bool resizeRequested{false};

    void createSwapchain(uint32_t width, uint32_t height);

    void resizeSwapchain();

private: // Draw Images
    // todo: In anticipation of deferred rendering, not implementing MSAA
    AllocatedImage drawImage{};
    AllocatedImage depthImage{};
    VkExtent2D drawExtent{};
    float renderScale{1.0f};
    float maxRenderScale{1.0f};

    void createDrawImages(uint32_t width, uint32_t height);

private: // Default Data
    AllocatedImage whiteImage;
    AllocatedImage errorCheckerboardImage;
    VkSampler defaultSamplerLinear{VK_NULL_HANDLE};
    VkSampler defaultSamplerNearest{VK_NULL_HANDLE};

private: // DearImgui
    VkDescriptorPool imguiPool{VK_NULL_HANDLE};

public: // Buffers
    AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) const;

    AllocatedBuffer createStagingBuffer(size_t allocSize) const;

    void copyBuffer(const AllocatedBuffer& src, const AllocatedBuffer& dst, VkDeviceSize size) const;

    VkDeviceAddress getBufferAddress(const AllocatedBuffer& buffer) const;

    void destroyBuffer(const AllocatedBuffer& buffer) const;

public: // Images
    AllocatedImage createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false) const;

    AllocatedImage createImage(const void *data, size_t dataSize, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                               bool mipmapped = false) const;

    static int getChannelCount(VkFormat format); // todo: move this static into vkhelpers
    void destroyImage(const AllocatedImage& img) const;
};

#endif //ENGINE_H
