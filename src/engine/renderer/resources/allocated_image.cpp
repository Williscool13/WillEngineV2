//
// Created by William on 2025-05-24.
//

#include "allocated_image.h"

#include "engine/renderer/vk_helpers.h"

namespace will_engine::renderer
{
AllocatedImage AllocatedImage::createInternal(VmaAllocator allocator, VkDevice device, const VkImageCreateInfo& imageInfo,
                                              const VmaAllocationCreateInfo& allocInfo, VkImageViewCreateInfo& viewInfo)
{

    AllocatedImage newImage{};
    newImage.imageFormat = imageInfo.format;
    newImage.imageExtent = imageInfo.extent;
    VK_CHECK(vmaCreateImage(allocator, &imageInfo, &allocInfo, &newImage.image, &newImage.allocation, nullptr));
    viewInfo.image = newImage.image;
    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &newImage.imageView));
    newImage.m_destroyed = false;
    return newImage;
}

AllocatedImage AllocatedImage::create(VmaAllocator allocator, VkDevice device, const VkImageCreateInfo& createInfo)
{
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    const VkImageAspectFlags aspectFlag = createInfo.format == VK_FORMAT_D32_SFLOAT || createInfo.format == VK_FORMAT_D32_SFLOAT_S8_UINT
                                              ? VK_IMAGE_ASPECT_DEPTH_BIT
                                              : VK_IMAGE_ASPECT_COLOR_BIT;

    // image ref will be added in createInternal
    VkImageViewCreateInfo viewInfo = vk_helpers::imageviewCreateInfo(createInfo.format, VK_NULL_HANDLE, aspectFlag);
    viewInfo.subresourceRange.levelCount = createInfo.mipLevels;
    return createInternal(allocator, device, createInfo, allocInfo, viewInfo);
}

AllocatedImage AllocatedImage::create(VmaAllocator allocator, VkDevice device, const VkExtent3D size, const VkFormat format,
                                      const VkImageUsageFlags usage, const bool mipmapped, VkImageAspectFlagBits aspectFlag)
{
    VkImageCreateInfo imgInfo = vk_helpers::imageCreateInfo(format, usage, size);
    if (mipmapped) {
        imgInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    VmaAllocationCreateInfo allocInfo{
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };

    VkImageAspectFlags targetAspect = aspectFlag;
    if (targetAspect == VK_IMAGE_ASPECT_NONE) {
        targetAspect = format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D32_SFLOAT_S8_UINT
                           ? VK_IMAGE_ASPECT_DEPTH_BIT
                           : VK_IMAGE_ASPECT_COLOR_BIT;
    }

    VkImageViewCreateInfo viewInfo = vk_helpers::imageviewCreateInfo(format, VK_NULL_HANDLE, targetAspect);
    viewInfo.subresourceRange.levelCount = imgInfo.mipLevels;
    return createInternal(allocator, device, imgInfo, allocInfo, viewInfo);
}

AllocatedImage AllocatedImage::create(VmaAllocator allocator, VkDevice device,const VkImageCreateInfo& bufferInfo, const VmaAllocationCreateInfo& allocInfo,
    VkImageViewCreateInfo& imageViewInfo)
{
    return createInternal(allocator, device, bufferInfo, allocInfo, imageViewInfo);
}

AllocatedImage AllocatedImage::createCubemap(VmaAllocator allocator, VkDevice device, const VkExtent3D size, const VkFormat format,
                                             const VkImageUsageFlags usage, const bool mipmapped)
{
    VkImageCreateInfo imgInfo = vk_helpers::cubemapCreateInfo(format, usage, size);
    if (mipmapped) {
        imgInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    VmaAllocationCreateInfo allocInfo = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };


    VkImageViewCreateInfo viewInfo = vk_helpers::cubemapViewCreateInfo(format, VK_NULL_HANDLE, VK_IMAGE_ASPECT_COLOR_BIT);
    viewInfo.subresourceRange.levelCount = imgInfo.mipLevels;

    return createInternal(allocator, device, imgInfo, allocInfo, viewInfo);
}

void AllocatedImage::release(VulkanContext& context)
{
    if (!m_destroyed) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(context.device, imageView, nullptr);
            imageView = VK_NULL_HANDLE;
        }
        if (image != VK_NULL_HANDLE) {
            vmaDestroyImage(context.allocator, image, allocation);
            image = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
        }
        imageExtent = {};
        imageFormat = {};
        m_destroyed = true;
    }
}

bool AllocatedImage::isValid() const
{
    return !m_destroyed && image != VK_NULL_HANDLE && imageView != VK_NULL_HANDLE;
}
}
