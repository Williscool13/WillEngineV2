//
// Created by William on 2025-05-24.
//

#include "buffer.h"

#include "engine/renderer/resource_manager.h"
#include "engine/renderer/vk_helpers.h"

namespace will_engine::renderer
{
Buffer::Buffer(ResourceManager* resourceManager, BufferType type, size_t size,
                                 VkBufferUsageFlags additionalUsages) : VulkanResource(resourceManager)
{
    auto [bufferUsage, allocFlags, memoryUsage, requiredFlags] = getBufferConfig(type, additionalUsages);

    const VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = bufferUsage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    const VmaAllocationCreateInfo allocInfo{
        .flags = allocFlags,
        .usage = memoryUsage,
        .requiredFlags = requiredFlags
    };

    VK_CHECK(vmaCreateBuffer(resourceManager->getAllocator(), &bufferInfo, &allocInfo, &buffer, &allocation, &info));
}

Buffer::Buffer(ResourceManager* resourceManager, size_t size, VkBufferUsageFlags usage,
                                 VmaMemoryUsage memoryUsage) : VulkanResource(resourceManager)
{
    const VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = size,
        .usage = usage
    };

    const VmaAllocationCreateInfo allocInfo = {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .usage = memoryUsage
    };

    VK_CHECK(vmaCreateBuffer(resourceManager->getAllocator(), &bufferInfo, &allocInfo, &buffer, &allocation, &info));
}

Buffer::BufferConfig Buffer::getBufferConfig(BufferType type, VkBufferUsageFlags additionalUsages)
{
    switch (type) {
        case BufferType::HostSequential:
            return {
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | additionalUsages,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                0
            };
        case BufferType::HostRandom:
            return {
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | additionalUsages,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                0
            };
        case BufferType::Device:
            return {
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | additionalUsages,
                0,
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            };
        case BufferType::Staging:
            return {
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT | additionalUsages,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                0
            };
        case BufferType::Receiving:
            return {
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | additionalUsages,
                0,
                VMA_MEMORY_USAGE_GPU_TO_CPU,
                0
            };
        default:
            return {};
    }
}

Buffer::~Buffer()
{
    if (buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(manager->getAllocator(), buffer, allocation);
        buffer = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
    }
    info = {};
}
}
