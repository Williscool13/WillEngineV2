//
// Created by William on 2024-12-07.
//

#ifndef VULKAN_CONTEXT_H
#define VULKAN_CONTEXT_H

#include <SDL.h>
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

class VulkanContext
{
public:
    VulkanContext(SDL_Window* window, bool useValidationLayers);
    /**
     * For Testing Only
     */
    VulkanContext() = default;
    ~VulkanContext();

    [[nodiscard]] VkInstance getInstance() const { return instance; }
    [[nodiscard]] VkDevice getDevice() const { return device; }
    [[nodiscard]] VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    [[nodiscard]] VmaAllocator getAllocator() const { return allocator; }
    [[nodiscard]] VkQueue getGraphicsQueue() const { return graphicsQueue; }
    [[nodiscard]] uint32_t getGraphicsQueueFamily() const { return graphicsQueueFamily; }
    [[nodiscard]] VkSurfaceKHR getSurface() const { return surface; }

//private:
    // public only for testing
public:

    VkInstance instance{};
    VkSurfaceKHR surface{};
    VkPhysicalDevice physicalDevice{};
    VkDevice device{};
    VkQueue graphicsQueue{};
    uint32_t graphicsQueueFamily{};
    VmaAllocator allocator{};
    VkDebugUtilsMessengerEXT debugMessenger{};
};

#endif //VULKAN_CONTEXT_H
