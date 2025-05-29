//
// Created by William on 2025-05-28.
//

#include "image_with_view.h"

#include "engine/renderer/resource_manager.h"
#include "engine/renderer/vk_helpers.h"

namespace will_engine::renderer
{
ImageWithView::ImageWithView(ResourceManager* resourceManager, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect,
                             bool mipmapped)
    : ImageResource(resourceManager)
{
    VkImageCreateInfo createInfo = vk_helpers::imageCreateInfo(format, usage, size);
    if (mipmapped) {
        createInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    constexpr VmaAllocationCreateInfo allocInfo{
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };

    imageFormat = createInfo.format;
    imageExtent = createInfo.extent;
    mipLevels = createInfo.mipLevels;
    VK_CHECK(vmaCreateImage(resourceManager->getAllocator(), &createInfo, &allocInfo, &image, &allocation, nullptr));

    VkImageViewCreateInfo viewInfo = vk_helpers::imageviewCreateInfo(format, VK_NULL_HANDLE, aspect);
    viewInfo.subresourceRange.levelCount = createInfo.mipLevels;
    viewInfo.image = image;
    VK_CHECK(vkCreateImageView(resourceManager->getDevice(), &viewInfo, nullptr, &imageView));
}

ImageWithView::ImageWithView(ResourceManager* resourceManager, const VkImageCreateInfo& createInfo, const VmaAllocationCreateInfo& allocInfo,
                             VkImageViewCreateInfo& viewInfo)
    : ImageResource(resourceManager)
{
    imageFormat = createInfo.format;
    imageExtent = createInfo.extent;
    mipLevels = createInfo.mipLevels;
    VK_CHECK(vmaCreateImage(resourceManager->getAllocator(), &createInfo, &allocInfo, &image, &allocation, nullptr));
    viewInfo.image = image;
    VK_CHECK(vkCreateImageView(resourceManager->getDevice(), &viewInfo, nullptr, &imageView));
}

ImageWithView::~ImageWithView()
{
    if (imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(manager->getDevice(), imageView, nullptr);
        imageView = VK_NULL_HANDLE;
    }

    if (image != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE) {
        vmaDestroyImage(manager->getAllocator(), image, allocation);
        image = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
    }

    imageExtent = {};
    imageFormat = VK_FORMAT_UNDEFINED;
    imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    mipLevels = 1;
}
}
