//
// Created by William on 2024-08-17.
// This class is a wrapper to simplify interaction for descriptor buffers in most general cases.
//

#ifndef VKDESCRIPTORBUFFER_H
#define VKDESCRIPTORBUFFER_H

#include <vector>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>
#include <fmt/format.h>

#include "vk_types.h"
#include "../util/vk_helpers.h"
#include "volk.h"


struct DescriptorImageData {
    VkDescriptorType type;
    VkDescriptorImageInfo *image_info;
    size_t count;
};

struct DescriptorUniformData {
    AllocatedBuffer uniformBuffer;
    size_t allocSize;
};


class DescriptorBuffer {
public:
    DescriptorBuffer() = delete;

    DescriptorBuffer(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, VmaAllocator allocator,
                     VkDescriptorSetLayout descriptorSetLayout, int maxObjectCount = 10);

    void destroy(VkDevice device, VmaAllocator allocator) const;

    void freeDescriptorBuffer(int index);

    /**
     * @return The size of a single descriptor set cached during Descriptor Buffer Initialization
     */
    [[nodiscard]] VkDeviceSize getDescriptorBufferSize() const { return descriptorBufferSize; }
    /**
     * @return The size of the Desriptor Buffer's offset cached during Descriptor Buffer Initialization
     */
    [[nodiscard]] VkDeviceSize getDescriptorBufferOffset() const { return descriptorBufferOffset; }

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


    std::vector<int> freeIndices;
    /**
     * Must be specified when this descriptor buffer is created.
     * \n Total descriptor buffer size will be offset + size * maxObjectCount
     */
    int maxObjectCount;

    /**
     * Physical device properties that are the same for all descriptor buffers (assuming device is the same)
     */
    static VkPhysicalDeviceDescriptorBufferPropertiesEXT deviceDescriptorBufferProperties;
    static bool devicePropertiesRetrieved;
};

class DescriptorBufferUniform : DescriptorBuffer {
public:
    DescriptorBufferUniform() = delete;

    /**
     *
     * @param instance
     * @param device
     * @param physicalDevice
     * @param allocator
     * @param descriptorSetLayout
     * @param maxObjectCount
     */
    DescriptorBufferUniform(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, VmaAllocator allocator,
                            VkDescriptorSetLayout descriptorSetLayout, int maxObjectCount = 10);

    /**
     * Allocates a descriptor set instance to a free index in the descriptor buffer. The vector passed to this should be equal in layout to
     * the descriptor set layout this descriptor buffer was initialized with.
     * @param device the device the uniform buffer is created in and this descriptor buffer was initialized with
     * @param uniformBuffers the uniform buffer and their corresponding total sizes to insert to the descriptor buffer
     * @return the index of the allocated descriptor set. Store and use when binding during draw call.
     */
    int setupData(VkDevice device, std::vector<DescriptorUniformData>& uniformBuffers);
};

class DescriptorBufferSampler : DescriptorBuffer {
public:
    DescriptorBufferSampler() = delete;

    DescriptorBufferSampler(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, VmaAllocator allocator,
                            VkDescriptorSetLayout descriptorSetLayout, int maxObjectCount = 10);

    /**
     * Allocates a descriptor set instance to a free index in the descriptor buffer. The vector passed to this should be equal in layout to
     * the descriptor set layout this descriptor buffer was initialized with.
     * @param device the device the uniform buffer is created in and this descriptor buffer was initialized with
     * @param imageBuffers the image/sampler data and how many times they should be registered (useful for padding)
     * @param index optional. If specified, will allocate/overwrite the descriptor set in that index of the descriptor buffer
     * @return the index of the allocated descriptor set. Store and use when binding during draw call.
     */
    int setupData(VkDevice device, std::vector<DescriptorImageData> imageBuffers, int index = -1);
};

#endif //VKDESCRIPTORBUFFER_H
