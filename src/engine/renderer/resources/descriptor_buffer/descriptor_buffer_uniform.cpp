//
// Created by William on 2025-01-22.
//

#include "descriptor_buffer_uniform.h"

#include "volk/volk.h"
#include "engine/renderer/vk_helpers.h"
#include "engine/renderer/vulkan_context.h"

using will_engine::DescriptorBufferException;

namespace will_engine::renderer
{
DescriptorBufferUniform DescriptorBufferUniform::create(const VulkanContext& ctx, VkDescriptorSetLayout descriptorSetLayout, int32_t maxObjectCount)
{
    DescriptorBufferUniform newDescriptorBuffer{};
    // Get size per Descriptor Set
    vkGetDescriptorSetLayoutSizeEXT(ctx.device, descriptorSetLayout, &newDescriptorBuffer.descriptorBufferSize);
    newDescriptorBuffer.descriptorBufferSize = vk_helpers::getAlignedSize(newDescriptorBuffer.descriptorBufferSize, ctx.deviceDescriptorBufferProperties.descriptorBufferOffsetAlignment);
    // Get Descriptor Buffer offset
    vkGetDescriptorSetLayoutBindingOffsetEXT(ctx.device, descriptorSetLayout, 0u, &newDescriptorBuffer.descriptorBufferOffset);

    newDescriptorBuffer.freeIndices.clear();
    for (int32_t i = 0; i < maxObjectCount; i++) { newDescriptorBuffer.freeIndices.insert(i); }
    newDescriptorBuffer.maxObjectCount = maxObjectCount;

    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = newDescriptorBuffer.descriptorBufferSize * maxObjectCount;
    bufferInfo.usage =
            VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VK_CHECK(
        vmaCreateBuffer(ctx.allocator, &bufferInfo, &vmaAllocInfo, &newDescriptorBuffer.buffer, &newDescriptorBuffer.allocation, &newDescriptorBuffer.info));

    newDescriptorBuffer.bufferAddress = vk_helpers::getDeviceAddress(ctx.device, newDescriptorBuffer.buffer);
    newDescriptorBuffer.m_destroyed = false;
    return newDescriptorBuffer;
}

int32_t DescriptorBufferUniform::setupData(const VulkanContext& ctx, const std::vector<DescriptorUniformData>& uniformBuffers, int32_t index)
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
        const VkDeviceAddress uniformBufferAddress = vk_helpers::getDeviceAddress(ctx.device, uniformBuffers[i].buffer);


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
        char* buffer_ptr_offset = static_cast<char*>(info.pMappedData) + accum_offset + descriptorBufferIndex *
                                  descriptorBufferSize;

        vkGetDescriptorEXT(ctx.device, &descriptorGetInfo, ctx.deviceDescriptorBufferProperties.uniformBufferDescriptorSize, buffer_ptr_offset);

        accum_offset += ctx.deviceDescriptorBufferProperties.uniformBufferDescriptorSize;
    }


    return descriptorBufferIndex;
}

VkBufferUsageFlagBits DescriptorBufferUniform::getBufferUsageFlags() const
{
    return VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;
}
}
