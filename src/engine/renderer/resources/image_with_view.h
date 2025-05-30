//
// Created by William on 2025-05-28.
//

#ifndef IMAGE_WITH_VIEW_H
#define IMAGE_WITH_VIEW_H

#include <vulkan/vulkan_core.h>
#include <vma/vk_mem_alloc.h>

#include "image_resource.h"

namespace will_engine::renderer
{
/**
 *
 */
struct ImageWithView final : ImageResource
{
    VmaAllocation allocation{VK_NULL_HANDLE};
    VkImageView imageView{VK_NULL_HANDLE};

    ImageWithView(ResourceManager* resourceManager, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, bool mipmapped = false);
    ImageWithView(ResourceManager* resourceManager, const VkImageCreateInfo& createInfo);
    ImageWithView(ResourceManager* resourceManager, const VkImageCreateInfo& createInfo, const VmaAllocationCreateInfo& allocInfo, VkImageViewCreateInfo& viewInfo);

    ~ImageWithView() override;
};
}

#endif //IMAGE_WITH_VIEW_H
