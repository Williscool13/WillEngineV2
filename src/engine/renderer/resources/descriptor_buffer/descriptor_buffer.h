//
// Created by William on 2024-08-17.
// This class is a wrapper to simplify interaction for descriptor buffers in most general cases.
//

#ifndef VKDESCRIPTORBUFFER_H
#define VKDESCRIPTORBUFFER_H

#include <unordered_set>

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
struct DescriptorBuffer : VulkanResource
{
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
    int32_t maxObjectCount{10};

public:
    explicit DescriptorBuffer(ResourceManager* resourceManager);

    ~DescriptorBuffer() override;

    void freeDescriptorBufferIndex(int32_t index);

    void freeAllDescriptorBufferIndices();

    VkDescriptorBufferBindingInfoEXT getBindingInfo() const;

    [[nodiscard]] VkDeviceSize getDescriptorBufferSize() const { return descriptorBufferSize; }

    bool isIndexOccupied(const int32_t index) const { return freeIndices.contains(index); }

protected:
    virtual VkBufferUsageFlagBits getBufferUsageFlags() const = 0;
};
}


#endif //VKDESCRIPTORBUFFER_H
