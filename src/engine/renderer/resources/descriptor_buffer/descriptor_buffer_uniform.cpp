//
// Created by William on 2025-01-22.
//

#include "descriptor_buffer_uniform.h"

#include "engine/renderer/resource_manager.h"
#include "volk/volk.h"
#include "engine/renderer/vk_helpers.h"
#include "engine/renderer/vulkan_context.h"

using will_engine::DescriptorBufferException;

namespace will_engine::renderer
{
DescriptorBufferUniform::DescriptorBufferUniform(ResourceManager* resourceManager, VkDescriptorSetLayout descriptorSetLayout, int32_t maxObjectCount)
    : DescriptorBuffer(resourceManager)
{
    // Get size per Descriptor Set
    vkGetDescriptorSetLayoutSizeEXT(resourceManager->getDevice(), descriptorSetLayout, &descriptorBufferSize);
    descriptorBufferSize = vk_helpers::getAlignedSize(descriptorBufferSize,
                                                      resourceManager->getPhysicalDeviceDescriptorBufferProperties().descriptorBufferOffsetAlignment);
    // Get Descriptor Buffer start ptr offset
    vkGetDescriptorSetLayoutBindingOffsetEXT(resourceManager->getDevice(), descriptorSetLayout, 0u, &descriptorBufferOffset);

    freeIndices.clear();
    for (int32_t i = 0; i < maxObjectCount; i++) { freeIndices.insert(i); }
    this->maxObjectCount = maxObjectCount;

    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = descriptorBufferSize * maxObjectCount;
    bufferInfo.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VK_CHECK(vmaCreateBuffer(resourceManager->getAllocator(), &bufferInfo, &vmaAllocInfo, &buffer, &allocation, &info));

    bufferAddress = vk_helpers::getDeviceAddress(resourceManager->getDevice(), buffer);
}

int32_t DescriptorBufferUniform::setupData(const std::span<DescriptorUniformData> uniformBuffers, const int32_t index)
{
    int32_t descriptorBufferIndex;
    if (index < 0) {
        if (freeIndices.empty()) {
            // TODO: Manage what happens if attempt to allocate a descriptor buffer set but out of space
            throw DescriptorBufferException("No space in DescriptorBufferSampler\n");
        }

        descriptorBufferIndex = *freeIndices.begin();
        freeIndices.erase(freeIndices.begin());
    }
    else {
        if (index >= maxObjectCount) {
            fmt::print("Specified index is higher than max allowed objects");
            return -1;
        }
        descriptorBufferIndex = index;
    }


    uint64_t accum_offset{descriptorBufferOffset};

    for (int32_t i = 0; i < uniformBuffers.size(); i++) {
        const VkDeviceAddress uniformBufferAddress = vk_helpers::getDeviceAddress(manager->getDevice(), uniformBuffers[i].buffer);

        VkDescriptorAddressInfoEXT descriptorAddressInfo = {};
        descriptorAddressInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
        descriptorAddressInfo.address = uniformBufferAddress;
        descriptorAddressInfo.range = uniformBuffers[i].allocSize;
        descriptorAddressInfo.format = VK_FORMAT_UNDEFINED;

        VkDescriptorGetInfoEXT descriptorGetInfo{};
        descriptorGetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        descriptorGetInfo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorGetInfo.data.pUniformBuffer = &descriptorAddressInfo;


        // at index 0, should be -> (pointer) + (baseOffset + 0 * uniformBufferSize) + (descriptorBufferIndex * descriptorBufferSize)
        // at index 1, should be -> (pointer) + (baseOffset + 1 * uniformBufferSize) + (descriptorBufferIndex * descriptorBufferSize)
        // at index 2, should be -> (pointer) + (baseOffset + 2 * uniformBufferSize) + (descriptorBufferIndex * descriptorBufferSize)
        char* buffer_ptr_offset = static_cast<char*>(info.pMappedData) + accum_offset + descriptorBufferIndex * descriptorBufferSize;

        const VkPhysicalDeviceDescriptorBufferPropertiesEXT& descriptorBufferProperties = manager->getPhysicalDeviceDescriptorBufferProperties();
        vkGetDescriptorEXT(manager->getDevice(), &descriptorGetInfo, descriptorBufferProperties.uniformBufferDescriptorSize, buffer_ptr_offset);

        accum_offset += descriptorBufferProperties.uniformBufferDescriptorSize;
    }


    return descriptorBufferIndex;
}

VkBufferUsageFlagBits DescriptorBufferUniform::getBufferUsageFlags() const
{
    return VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;
}
}
