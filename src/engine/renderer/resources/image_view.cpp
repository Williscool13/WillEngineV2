//
// Created by William on 2025-05-25.
//

#include "image_view.h"

#include <volk/volk.h>

#include "engine/renderer/resource_manager.h"
#include "engine/renderer/vk_helpers.h"

namespace will_engine::renderer
{
ImageView::ImageView(ResourceManager* resourceManager, const VkImageViewCreateInfo& createInfo) : VulkanResource(resourceManager)
{
    VK_CHECK(vkCreateImageView(resourceManager->getDevice(), &createInfo, nullptr, &imageView));
}

ImageView::~ImageView()
{
    if (imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(manager->getDevice(), imageView, nullptr);
        imageView = VK_NULL_HANDLE;
    }
}
}
