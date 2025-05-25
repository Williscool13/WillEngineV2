//
// Created by William on 2024-08-17.
// This class is a wrapper to simplify interaction for descriptor buffers in most general cases.
//

#ifndef VKDESCRIPTORBUFFER_H
#define VKDESCRIPTORBUFFER_H

#include <unordered_set>
#include <utility>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include "engine/renderer/vk_types.h"
#include "engine/renderer/resources/vulkan_resource.h"

namespace will_engine
{
class VulkanContext;
}

namespace will_engine::renderer
{
class DescriptorBuffer : public VulkanResource
{
public:
    DescriptorBuffer() = default;

    ~DescriptorBuffer() override = default;

    // No copying
    DescriptorBuffer(const DescriptorBuffer&) = delete;

    DescriptorBuffer& operator=(const DescriptorBuffer&) = delete;

    DescriptorBuffer(DescriptorBuffer&& other) noexcept
        : buffer(std::exchange(other.buffer, VK_NULL_HANDLE))
          , allocation(std::exchange(other.allocation, VK_NULL_HANDLE))
          , info(std::exchange(other.info, {}))
          , bufferAddress(std::exchange(other.bufferAddress, 0))
          , descriptorBufferSize(other.descriptorBufferSize)
          , descriptorBufferOffset(other.descriptorBufferOffset)
          , freeIndices(std::move(other.freeIndices))
          , maxObjectCount(other.maxObjectCount)
          , m_destroyed(other.m_destroyed)
    {
        other.m_destroyed = true;
    }

    DescriptorBuffer& operator=(DescriptorBuffer&& other) noexcept
    {
        if (this != &other) {
            buffer = std::exchange(other.buffer, VK_NULL_HANDLE);
            allocation = std::exchange(other.allocation, VK_NULL_HANDLE);
            info = std::exchange(other.info, {});
            bufferAddress = std::exchange(other.bufferAddress, 0);
            descriptorBufferSize = other.descriptorBufferSize;
            descriptorBufferOffset = other.descriptorBufferOffset;
            freeIndices = std::move(other.freeIndices);
            maxObjectCount = other.maxObjectCount;
            m_destroyed = other.m_destroyed;
            other.m_destroyed = true;
        }
        return *this;
    }

    void release(VulkanContext& context) override;

    bool isValid() const override;

    void freeDescriptorBufferIndex(int32_t index);

    void freeAllDescriptorBufferIndices();

    VkDescriptorBufferBindingInfoEXT getBindingInfo() const;

    [[nodiscard]] VkDeviceSize getDescriptorBufferSize() const { return descriptorBufferSize; }

    bool isIndexOccupied(const int32_t index) const { return freeIndices.contains(index); }

protected:
    virtual VkBufferUsageFlagBits getBufferUsageFlags() const = 0;

protected:
    VkBuffer buffer{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
    VmaAllocationInfo info{};

    VkDeviceAddress bufferAddress{};
    /**
     * The size of 1 descriptor set
     */
    VkDeviceSize descriptorBufferSize{};
    /**
     * The offset for potential metadata of the buffer
     */
    VkDeviceSize descriptorBufferOffset{};

    std::unordered_set<int32_t> freeIndices;
    /**
     * Must be specified when this descriptor buffer is created.
     * \n Total descriptor buffer size will be offset + size * maxObjectCount
     */
    int32_t maxObjectCount{10};

    bool m_destroyed{true};
};
}


#endif //VKDESCRIPTORBUFFER_H
