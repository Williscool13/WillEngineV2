//
// Created by William on 8/11/2024.
//

#ifndef ENGINE_H
#define ENGINE_H

#include <chrono>
#include <deque>
#include <functional>

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
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include "glm/detail/func_packing.inl"

#include "VkTypes.h"
#include "VkDescriptors.h"
#include "../util/VkHelpers.h"

constexpr unsigned int FRAME_OVERLAP = 2;


struct DeletionQueue {
    std::deque<std::function<void()> > deletors;

    void pushFunction(std::function<void()> &&function) {
        deletors.push_back(function);
    }

    void flush() {
        // reverse iterate the deletion queue to execute all the functions
        for (auto& it : deletors) {
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

    void cleanup();

private:
    VkExtent2D windowExtent{1700, 900};
    struct SDL_Window *window{nullptr};

    VkInstance instance{};
    VkSurfaceKHR surface{};
    VkPhysicalDevice physicalDevice{};
    VkDevice device{};
    VkQueue graphicsQueue{};
    uint32_t graphicsQueueFamily{};
    VmaAllocator allocator{};
    VkDebugUtilsMessengerEXT debug_messenger{};


private: // Rendering
    // Main
    int frameNumber{0};
    FrameData frames[FRAME_OVERLAP]{};
    FrameData &getCurrentFrame() { return frames[frameNumber % FRAME_OVERLAP]; };

    // Immediate Mode
    VkFence immFence{VK_NULL_HANDLE};
    VkCommandBuffer immCommandBuffer{VK_NULL_HANDLE};
    VkCommandPool immCommandPool{VK_NULL_HANDLE};
    //void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

private: // Swapchain
    VkSwapchainKHR swapchain{};
    VkFormat swapchainImageFormat{};
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkExtent2D swapchainExtent{};

    void createSwapchain(uint32_t width, uint32_t height);

private: // Draw Images
    // todo: In anticipation of deferred rendering, not implementing MSAA
    AllocatedImage drawImage{};
    AllocatedImage depthImage{};
    VkExtent2D drawExtent{};
    float renderScale{ 1.0f };
    float maxRenderScale{ 1.0f };

    void createDrawImages(uint32_t width, uint32_t height);

private: // Default Data
    AllocatedImage whiteImage;
    AllocatedImage errorCheckerboardImage;
    VkSampler defaultSamplerLinear{VK_NULL_HANDLE};
    VkSampler defaultSamplerNearest{VK_NULL_HANDLE};

private: // DearImgui
    VkDescriptorPool imguiPool{VK_NULL_HANDLE};

private: // Initialization
    void initVulkan();

    void initSwapchain();

    void initCommands();

    void initSyncStructures();

    void initDefaultData();

    void initDearImgui();

    void initPipelines();

    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function) const;

public: // Buffers
    AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    AllocatedBuffer createStagingBuffer(size_t allocSize);
    void copyBuffer(AllocatedBuffer src, AllocatedBuffer dst, VkDeviceSize size);
    VkDeviceAddress getBufferAddress(AllocatedBuffer buffer);
    void destroyBuffer(const AllocatedBuffer& buffer);

public: // Images
    AllocatedImage createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    AllocatedImage createImage(void* data, size_t dataSize, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    static int getChannelCount(VkFormat format); // todo: move this static into vkhelpers
    void destroyImage(const AllocatedImage& img);
};

#endif //ENGINE_H
