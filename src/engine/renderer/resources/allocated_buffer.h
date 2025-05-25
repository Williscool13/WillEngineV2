//
// Created by William on 2025-05-24.
//

#ifndef ALLOCATED_BUFFER_H
#define ALLOCATED_BUFFER_H
#include <cassert>
#include <utility>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include "vulkan_resource.h"
#include "engine/renderer/vulkan_context.h"

namespace will_engine::renderer
{
/**
 * A wrapper for a VkBuffer with VMA allocation.
 */
class AllocatedBuffer final : public VulkanResource
{
public:
    VkBuffer buffer{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
    VmaAllocationInfo info{};

    AllocatedBuffer() = default;

    ~AllocatedBuffer() override
    {
        assert(m_destroyed && "Resource not properly destroyed before destructor!");
    }

    void release(VulkanContext& context) override;

    bool isValid() const override;

    static AllocatedBuffer create(VmaAllocator allocator, size_t allocSize,
                                  VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

    static AllocatedBuffer createHostSequential(VmaAllocator allocator, size_t allocSize,
                                                VkBufferUsageFlags additionalUsages = 0);

    static AllocatedBuffer createHostRandom(VmaAllocator allocator, size_t allocSize,
                                            VkBufferUsageFlags additionalUsages = 0);

    static AllocatedBuffer createDevice(VmaAllocator allocator, size_t allocSize,
                                        VkBufferUsageFlags additionalUsages = 0);

    static AllocatedBuffer createStaging(VmaAllocator allocator, size_t allocSize);

    static AllocatedBuffer createReceiving(VmaAllocator allocator, size_t allocSize);

    // No copying
    AllocatedBuffer(const AllocatedBuffer&) = delete;

    AllocatedBuffer& operator=(const AllocatedBuffer&) = delete;

    // Move constructor
    AllocatedBuffer(AllocatedBuffer&& other) noexcept
        : buffer(std::exchange(other.buffer, VK_NULL_HANDLE))
          , allocation(std::exchange(other.allocation, VK_NULL_HANDLE))
          , info(std::exchange(other.info, {}))
          , m_destroyed(other.m_destroyed)
    {
        other.m_destroyed = true;
    }

    AllocatedBuffer& operator=(AllocatedBuffer&& other) noexcept
    {
        if (this != &other) {
            buffer = std::exchange(other.buffer, VK_NULL_HANDLE);
            allocation = std::exchange(other.allocation, VK_NULL_HANDLE);
            info = std::exchange(other.info, {});
            m_destroyed = other.m_destroyed;
            other.m_destroyed = true;
        }
        return *this;
    }



private:
    static AllocatedBuffer createInternal(VmaAllocator allocator, const VkBufferCreateInfo& bufferInfo, const VmaAllocationCreateInfo& allocInfo);

private:
    bool m_destroyed{true};
};
}

#endif //ALLOCATED_BUFFER_H
