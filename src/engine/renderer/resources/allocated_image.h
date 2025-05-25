//
// Created by William on 2025-05-24.
//

#ifndef ALLOCATED_IMAGE_H
#define ALLOCATED_IMAGE_H
#include <cassert>
#include <utility>
#include <vma/vk_mem_alloc.h>
#include <volk/volk.h>
#include <vulkan/vulkan_core.h>

#include "vulkan_resource.h"
#include "engine/renderer/vulkan_context.h"

namespace will_engine::renderer
{
/**
 * A wrapper for a VkImage that always comes bundled with a VkImageView.
 */
class AllocatedImage : public VulkanResource
{
public:
    VkImage image{VK_NULL_HANDLE};
    VkImageView imageView{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
    VkExtent3D imageExtent{};
    VkFormat imageFormat{VK_FORMAT_UNDEFINED};

    AllocatedImage() = default;

    static AllocatedImage create(VmaAllocator allocator, VkDevice device,
                                 const VkImageCreateInfo& createInfo);

    static AllocatedImage create(VmaAllocator allocator, VkDevice device,
                                 VkExtent3D size, VkFormat format,
                                 VkImageUsageFlags usage, bool mipmapped = false,
                                 VkImageAspectFlagBits aspectFlag = VK_IMAGE_ASPECT_NONE);

    static AllocatedImage create(VmaAllocator allocator, VkDevice device,
                                 const VkImageCreateInfo& bufferInfo, const VmaAllocationCreateInfo& allocInfo, VkImageViewCreateInfo& imageViewInfo);

    static AllocatedImage createCubemap(VmaAllocator allocator, VkDevice device,
                                        VkExtent3D size, VkFormat format,
                                        VkImageUsageFlags usage, bool mipmapped = false);

    // No copying
    AllocatedImage(const AllocatedImage&) = delete;

    AllocatedImage& operator=(const AllocatedImage&) = delete;

    AllocatedImage(AllocatedImage&& other) noexcept
        : image(std::exchange(other.image, VK_NULL_HANDLE))
          , imageView(std::exchange(other.imageView, VK_NULL_HANDLE))
          , allocation(std::exchange(other.allocation, VK_NULL_HANDLE))
          , imageExtent(other.imageExtent)
          , imageFormat(other.imageFormat)
          , m_destroyed(other.m_destroyed)
    {
        other.m_destroyed = true;
    }

    AllocatedImage& operator=(AllocatedImage&& other) noexcept
    {
        if (this != &other) {
            image = std::exchange(other.image, VK_NULL_HANDLE);
            imageView = std::exchange(other.imageView, VK_NULL_HANDLE);
            allocation = std::exchange(other.allocation, VK_NULL_HANDLE);
            imageExtent = other.imageExtent;
            imageFormat = other.imageFormat;
            m_destroyed = other.m_destroyed;

            other.m_destroyed = true;
        }
        return *this;
    }

    ~AllocatedImage() override
    {
        assert(m_destroyed && "Resource not properly destroyed before destructor!");
    }

    void release(VulkanContext& context) override;

    bool isValid() const override;

private:
    static AllocatedImage createInternal(VmaAllocator allocator, VkDevice device,
                                         const VkImageCreateInfo& imageInfo,
                                         const VmaAllocationCreateInfo& allocInfo,
                                         VkImageViewCreateInfo& viewInfo);

private:
    bool m_destroyed{true};
};
}

#endif //ALLOCATED_IMAGE_H
