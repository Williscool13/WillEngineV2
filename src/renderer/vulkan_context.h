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
    ~VulkanContext();

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
