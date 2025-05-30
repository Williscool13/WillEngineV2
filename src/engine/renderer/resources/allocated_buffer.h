//
// Created by William on 2025-05-24.
//

#ifndef ALLOCATED_BUFFER_H
#define ALLOCATED_BUFFER_H

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include "vulkan_resource.h"

namespace will_engine::renderer
{
enum class BufferType
{
    HostSequential,
    HostRandom,
    Device,
    Staging,
    Receiving,
    Custom
};

/**
 * A wrapper for a VkBuffer with VMA allocation.
 */
struct AllocatedBuffer final : VulkanResource
{
    VkBuffer buffer{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
    VmaAllocationInfo info{};


    AllocatedBuffer(ResourceManager* resourceManager, BufferType type, size_t size, VkBufferUsageFlags additionalUsages = 0);

    AllocatedBuffer(ResourceManager* resourceManager, size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

    ~AllocatedBuffer() override;

private:
    struct BufferConfig
    {
        VkBufferUsageFlags usage;
        VmaAllocationCreateFlags allocFlags;
        VmaMemoryUsage memoryUsage;
        VkMemoryPropertyFlags requiredFlags;
    };

    static BufferConfig getBufferConfig(BufferType type, VkBufferUsageFlags additionalUsages);
};
}

#endif //ALLOCATED_BUFFER_H
