//
// Created by William on 2025-05-27.
//

#ifndef IMAGE_RESOURCE_H
#define IMAGE_RESOURCE_H

#include <vulkan/vulkan_core.h>

#include "vulkan_resource.h"


namespace will_engine::renderer
{
struct ImageResource : VulkanResource
{
    VkImage image{VK_NULL_HANDLE};
    VkFormat imageFormat{VK_FORMAT_UNDEFINED};
    VkExtent3D imageExtent{};
    VkImageLayout imageLayout{VK_IMAGE_LAYOUT_UNDEFINED};
    uint32_t mipLevels{1};

    explicit ImageResource(ResourceManager* resourceManager) : VulkanResource(resourceManager) {}

    ~ImageResource() override = 0;
};
}


#endif //IMAGE_RESOURCE_H
