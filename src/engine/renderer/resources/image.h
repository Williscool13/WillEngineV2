//
// Created by William on 2025-05-27.
//

#ifndef IMAGE_H
#define IMAGE_H

#include <vulkan/vulkan_core.h>
#include <vma/vk_mem_alloc.h>

#include "image_resource.h"

namespace will_engine::renderer
{
class Image final : public ImageResource
{
public:
    VmaAllocation allocation{VK_NULL_HANDLE};

    Image(ResourceManager* resourceManager, const VkImageCreateInfo& createInfo);

    Image(ResourceManager* resourceManager, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);

    Image(ResourceManager* resourceManager, const VkImageCreateInfo& createInfo, const VmaAllocationCreateInfo& allocInfo);

    ~Image() override;
};
}

#endif //IMAGE_H
