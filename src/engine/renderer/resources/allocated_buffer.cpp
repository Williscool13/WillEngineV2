//
// Created by William on 2025-05-24.
//

#include "allocated_buffer.h"

#include "engine/renderer/vk_helpers.h"

namespace will_engine::renderer
{
void AllocatedBuffer::release(VulkanContext& context)
{
    if (!m_destroyed) {
        if (buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(context.allocator, buffer, allocation);
            buffer = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
        }
        info = {};
        m_destroyed = true;
    }
}

bool AllocatedBuffer::isValid() const
{
    return !m_destroyed && buffer != VK_NULL_HANDLE;
}

AllocatedBuffer AllocatedBuffer::createInternal(VmaAllocator allocator, const VkBufferCreateInfo& bufferInfo,
                                                const VmaAllocationCreateInfo& allocInfo)
{
    AllocatedBuffer newBuffer{};
    VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));
    newBuffer.m_destroyed = false;
    return newBuffer;
}

AllocatedBuffer AllocatedBuffer::create(VmaAllocator allocator, size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = allocSize,
        .usage = usage
    };

    VmaAllocationCreateInfo vmaallocInfo = {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .usage = memoryUsage
    };

    return createInternal(allocator, bufferInfo, vmaallocInfo);
}

AllocatedBuffer AllocatedBuffer::createHostSequential(VmaAllocator allocator, size_t allocSize, VkBufferUsageFlags additionalUsages)
{
    const VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = allocSize,
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | additionalUsages,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    constexpr VmaAllocationCreateInfo allocInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
    };

    return createInternal(allocator, bufferInfo, allocInfo);
}

AllocatedBuffer AllocatedBuffer::createHostRandom(VmaAllocator allocator, size_t allocSize, VkBufferUsageFlags additionalUsages)
{
    const VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = allocSize,
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | additionalUsages,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    constexpr VmaAllocationCreateInfo allocInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
    };

    return createInternal(allocator, bufferInfo, allocInfo);
}

AllocatedBuffer AllocatedBuffer::createDevice(VmaAllocator allocator, size_t allocSize, VkBufferUsageFlags additionalUsages)
{
    const VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = allocSize,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | additionalUsages,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    constexpr VmaAllocationCreateInfo allocInfo{
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    return createInternal(allocator, bufferInfo, allocInfo);
}

AllocatedBuffer AllocatedBuffer::createStaging(VmaAllocator allocator, size_t allocSize)
{
    const VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = allocSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    constexpr VmaAllocationCreateInfo allocInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
    };

    return createInternal(allocator, bufferInfo, allocInfo);
}

AllocatedBuffer AllocatedBuffer::createReceiving(VmaAllocator allocator, size_t allocSize)
{
    return create(allocator, allocSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU);
}
}
