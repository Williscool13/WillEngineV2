//
// Created by William on 2025-05-25.
//

#ifndef IMAGE_VIEW_H
#define IMAGE_VIEW_H

#include <vulkan/vulkan_core.h>

#include "vulkan_resource.h"

namespace will_engine::renderer
{
/**
* A wrapper for a VkImageView.
* \n Note that it is the responsibility of the programmer to ensure that the VkImage that this ImageView references is still alive when this is accessed.
*/
struct ImageView : VulkanResource
{
    VkImageView imageView{VK_NULL_HANDLE};

    ImageView(ResourceManager* resourceManager, const VkImageViewCreateInfo& createInfo);

    ~ImageView() override;
};
}

#endif //IMAGE_VIEW_H
