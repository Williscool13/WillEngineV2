//
// Created by William on 2025-05-25.
//

#include "image_view.h"

#include <volk/volk.h>

#include "engine/renderer/vk_helpers.h"

namespace will_engine::renderer
{
void ImageView::release(VulkanContext& context)
{
    if (!m_destroyed) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(context.device, imageView, nullptr);
            imageView = VK_NULL_HANDLE;
        }
        m_destroyed = true;
    }
}

bool ImageView::isValid() const
{
    return !m_destroyed && imageView != VK_NULL_HANDLE;
}

ImageView ImageView::create(VkDevice device, const VkImageViewCreateInfo& createInfo)
{
    ImageView newImagView{};
    VK_CHECK(vkCreateImageView(device, &createInfo, nullptr, &newImagView.imageView));
    newImagView.m_destroyed = false;
    return newImagView;
}
}
