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


class VulkanContext;

namespace will_engine
{
class DescriptorBuffer
{
public:
    DescriptorBuffer() = default;

    virtual ~DescriptorBuffer() = default;

    DescriptorBuffer(const VulkanContext& context, VkDescriptorSetLayout descriptorSetLayout, int32_t maxObjectCount = 10);

    void freeDescriptorBufferIndex(int32_t index);

    void freeAllDescriptorBufferIndices();

    VkDescriptorBufferBindingInfoEXT getDescriptorBufferBindingInfo() const;

    [[nodiscard]] VkDeviceSize getDescriptorBufferSize() const { return descriptorBufferSize; }

    bool isIndexOccupied(const int32_t index) const { return freeIndices.contains(index); }

    AllocatedBuffer& getBuffer() { return mainBuffer; }

protected:
    /**
     * The underlying VkBuffer for this descriptor buffer
     */
    AllocatedBuffer mainBuffer{VK_NULL_HANDLE};
    VkDeviceAddress mainBufferGpuAddress{};

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

    virtual VkBufferUsageFlagBits getBufferUsageFlags() const = 0;

    /**
     * Physical device properties that are the same for all descriptor buffers (assuming device is the same)
     */
    static VkPhysicalDeviceDescriptorBufferPropertiesEXT deviceDescriptorBufferProperties;
    static bool devicePropertiesRetrieved;
};
}


#endif //VKDESCRIPTORBUFFER_H
