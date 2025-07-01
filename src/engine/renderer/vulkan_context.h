//
// Created by William on 2024-12-07.
//

#ifndef VULKAN_CONTEXT_H
#define VULKAN_CONTEXT_H

#include <SDL.h>
#include <vulkan/vulkan_core.h>
#include <vma/vk_mem_alloc.h>

namespace will_engine::renderer
{
class VulkanContext
{
public:
    VulkanContext(SDL_Window* window, bool useValidationLayers);

    ~VulkanContext();

    // No copying
    VulkanContext(const VulkanContext&) = delete;

    VulkanContext& operator=(const VulkanContext&) = delete;

    VkInstance instance{};
    VkSurfaceKHR surface{};
    VkPhysicalDevice physicalDevice{};
    VkDevice device{};
    VkQueue graphicsQueue{};
    uint32_t graphicsQueueFamily{};
    VmaAllocator allocator{};
    VkDebugUtilsMessengerEXT debugMessenger{};

    VkPhysicalDeviceDescriptorBufferPropertiesEXT deviceDescriptorBufferProperties{};
};
}


#endif //VULKAN_CONTEXT_H
