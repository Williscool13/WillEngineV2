//
// Created by William on 2025-05-27.
//

#include "image.h"

#include "engine/renderer/resource_manager.h"
#include "engine/renderer/vk_helpers.h"

namespace will_engine::renderer
{
Image::Image(ResourceManager* resourceManager, const VkImageCreateInfo& createInfo)
    : ImageResource(resourceManager)
{
    constexpr VmaAllocationCreateInfo allocInfo{
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    imageFormat = createInfo.format;
    imageExtent = createInfo.extent;
    mipLevels = createInfo.mipLevels;
    VK_CHECK(vmaCreateImage(resourceManager->getAllocator(), &createInfo, &allocInfo, &image, &allocation, nullptr));

    VkImageAspectFlags aspectFlag;
    if (createInfo.format == VK_FORMAT_D32_SFLOAT) {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    } else if (createInfo.format == VK_FORMAT_D24_UNORM_S8_UINT) {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    } else {
        aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    VkImageViewCreateInfo viewInfo = vk_helpers::imageviewCreateInfo(createInfo.format, VK_NULL_HANDLE, aspectFlag);
    viewInfo.subresourceRange.levelCount = createInfo.mipLevels;
    viewInfo.image = image;
    VK_CHECK(vkCreateImageView(resourceManager->getDevice(), &viewInfo, nullptr, &imageView));
}

Image::Image(ResourceManager* resourceManager, const VkExtent3D size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped)
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
    VkImageAspectFlags aspectFlag;
    if (createInfo.format == VK_FORMAT_D32_SFLOAT) {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    } else if (createInfo.format == VK_FORMAT_D24_UNORM_S8_UINT) {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    } else {
        aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    VkImageViewCreateInfo viewInfo = vk_helpers::imageviewCreateInfo(createInfo.format, VK_NULL_HANDLE, aspectFlag);
    viewInfo.subresourceRange.levelCount = createInfo.mipLevels;
    viewInfo.image = image;
    VK_CHECK(vkCreateImageView(resourceManager->getDevice(), &viewInfo, nullptr, &imageView));
}

Image::Image(ResourceManager* resourceManager, const VkImageCreateInfo& createInfo, const VmaAllocationCreateInfo& allocInfo,
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

Image::~Image()
{
    if (image != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE) {
        vmaDestroyImage(manager->getAllocator(), image, allocation);
        image = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
    }
    if (imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(manager->getDevice(), imageView, nullptr);
        imageView = VK_NULL_HANDLE;
    }
    imageExtent = {};
    imageFormat = VK_FORMAT_UNDEFINED;
    imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    mipLevels = 1;
}
}
