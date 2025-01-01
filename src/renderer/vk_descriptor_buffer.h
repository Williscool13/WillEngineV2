//
// Created by William on 2024-08-17.
// This class is a wrapper to simplify interaction for descriptor buffers in most general cases.
//

#ifndef VKDESCRIPTORBUFFER_H
#define VKDESCRIPTORBUFFER_H

#include <unordered_set>
#include <vector>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>
#include <fmt/format.h>

#include "vk_types.h"
#include "vk_helpers.h"


class VulkanContext;

struct DescriptorImageData
{
    VkDescriptorType type{VK_DESCRIPTOR_TYPE_SAMPLER};
    VkDescriptorImageInfo imageInfo{};
    bool bIsPadding{false};
};

struct DescriptorUniformData
{
    AllocatedBuffer uniformBuffer{};
    size_t allocSize{};
};


class DescriptorBuffer
{
public:
    DescriptorBuffer() = default;

    virtual ~DescriptorBuffer() = default;

    DescriptorBuffer(const VulkanContext& context, VkDescriptorSetLayout descriptorSetLayout, int maxObjectCount = 10);

    void destroy(VmaAllocator allocator) const;

    void freeDescriptorBuffer(int index);

    VkDescriptorBufferBindingInfoEXT getDescriptorBufferBindingInfo() const;

    [[nodiscard]] VkDeviceSize getDescriptorBufferSize() const { return descriptorBufferSize; }

    bool isIndexOccupied(const int index) const { return freeIndices.contains(index); }

protected:
    /**
     * The underlying VkBuffer for this descriptor buffer
     */
    AllocatedBuffer descriptorBuffer{VK_NULL_HANDLE};
    VkDeviceAddress descriptorBufferGpuAddress{};
    VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};

    /**
     * The size of 1 descriptor set
     */
    VkDeviceSize descriptorBufferSize{};
    /**
     * The offset for potential metadata of the buffer
     */
    VkDeviceSize descriptorBufferOffset{};


    std::unordered_set<int> freeIndices;
    /**
     * Must be specified when this descriptor buffer is created.
     * \n Total descriptor buffer size will be offset + size * maxObjectCount
     */
    int maxObjectCount{10};

    virtual VkBufferUsageFlagBits getBufferUsageFlags() const = 0;

    /**
     * Physical device properties that are the same for all descriptor buffers (assuming device is the same)
     */
    static VkPhysicalDeviceDescriptorBufferPropertiesEXT deviceDescriptorBufferProperties;
    static bool devicePropertiesRetrieved;
};

class DescriptorBufferUniform final : public DescriptorBuffer
{
public:
    DescriptorBufferUniform() = default;

    /**
     *
     * @param context
     * @param descriptorSetLayout
     * @param maxObjectCount
     */
    DescriptorBufferUniform(const VulkanContext& context, VkDescriptorSetLayout descriptorSetLayout, int maxObjectCount = 10);

    /**
     * Allocates a descriptor set instance to a free index in the descriptor buffer. The vector passed to this should be equal in layout to
     * the descriptor set layout this descriptor buffer was initialized with.
     * @param device the device the uniform buffer is created in and this descriptor buffer was initialized with
     * @param uniformBuffers the uniform buffer and their corresponding total sizes to insert to the descriptor buffer
     * @param index
     * @return the index of the allocated descriptor set. Store and use when binding during draw call.
     */
    int32_t setupData(VkDevice device, const std::vector<DescriptorUniformData>& uniformBuffers, int index = -1);

    VkBufferUsageFlagBits getBufferUsageFlags() const override;
};

class DescriptorBufferSampler final : public DescriptorBuffer
{
public:
    DescriptorBufferSampler() = default;

    DescriptorBufferSampler(const VulkanContext& context, VkDescriptorSetLayout descriptorSetLayout, int maxObjectCount = 10);

    /**
     * Allocates a descriptor set instance to a free index in the descriptor buffer. The vector passed to this should be equal in layout to
     * the descriptor set layout this descriptor buffer was initialized with.
     * @param device the device the uniform buffer is created in and this descriptor buffer was initialized with
     * @param imageBuffers the image/sampler data and how many times they should be registered (useful for padding)
     * @param index optional. If specified, will allocate/overwrite the descriptor set in that index of the descriptor buffer
     * @return the index of the allocated descriptor set. Store and use when binding during draw call.
     */
    int setupData(VkDevice device, const std::vector<DescriptorImageData>& imageBuffers, int index = -1);

    VkBufferUsageFlagBits getBufferUsageFlags() const override;
};

#endif //VKDESCRIPTORBUFFER_H
