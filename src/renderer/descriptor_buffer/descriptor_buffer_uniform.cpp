//
// Created by William on 2025-01-22.
//

#include "descriptor_buffer_uniform.h"

#include "volk/volk.h"
#include "src/renderer/vk_helpers.h"
#include "src/renderer/vulkan_context.h"

using will_engine::DescriptorBufferException;

will_engine::DescriptorBufferUniform::DescriptorBufferUniform(const VulkanContext& context, VkDescriptorSetLayout descriptorSetLayout, int32_t maxObjectCount)
    : DescriptorBuffer(context, descriptorSetLayout, maxObjectCount)
{
    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = descriptorBufferSize * maxObjectCount;
    bufferInfo.usage =
            VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VK_CHECK(vmaCreateBuffer(context.allocator, &bufferInfo, &vmaAllocInfo, &mainBuffer.buffer, &mainBuffer.allocation, &mainBuffer.info));

    mainBufferGpuAddress = vk_helpers::getDeviceAddress(context.device, mainBuffer.buffer);
}

int32_t will_engine::DescriptorBufferUniform::setupData(VkDevice device, const std::vector<DescriptorUniformData>& uniformBuffers, int32_t index)
{
    int32_t descriptorBufferIndex;
    if (index < 0) {
        if (freeIndices.empty()) {
            // TODO: Manage what happens if attempt to allocate a descriptor buffer set but out of space
            throw DescriptorBufferException("No space in DescriptorBufferSampler\n");
        }

        descriptorBufferIndex = *freeIndices.begin();
        freeIndices.erase(freeIndices.begin());
    } else {
        if (index >= maxObjectCount) {
            fmt::print("Specified index is higher than max allowed objects");
            return -1;
        }
        descriptorBufferIndex = index;
    }


    uint64_t accum_offset{descriptorBufferOffset};

    for (int32_t i = 0; i < uniformBuffers.size(); i++) {
        const VkDeviceAddress uniformBufferAddress = vk_helpers::getDeviceAddress(device, uniformBuffers[i].uniformBuffer.buffer);


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
        char* buffer_ptr_offset = static_cast<char*>(mainBuffer.info.pMappedData) + accum_offset + descriptorBufferIndex *
                                  descriptorBufferSize;

        vkGetDescriptorEXT(device, &descriptorGetInfo, deviceDescriptorBufferProperties.uniformBufferDescriptorSize, buffer_ptr_offset);

        accum_offset += deviceDescriptorBufferProperties.uniformBufferDescriptorSize;
    }


    return descriptorBufferIndex;
}

VkBufferUsageFlagBits will_engine::DescriptorBufferUniform::getBufferUsageFlags() const
{
    return VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;
}
