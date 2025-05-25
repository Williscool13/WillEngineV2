//
// Created by William on 2025-01-22.
//

#include "descriptor_buffer_sampler.h"

#include "volk/volk.h"
#include "engine/renderer/vk_helpers.h"
#include "engine/renderer/vulkan_context.h"

using will_engine::DescriptorImageData;
using will_engine::DescriptorBufferException;

namespace will_engine::renderer
{
DescriptorBufferSampler DescriptorBufferSampler::create(const VulkanContext& ctx, VkDescriptorSetLayout descriptorSetLayout, int32_t maxObjectCount)
{
    DescriptorBufferSampler newDescriptorBuffer{};
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
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
            | VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;
    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VK_CHECK(vmaCreateBuffer(ctx.allocator, &bufferInfo, &vmaAllocInfo, &newDescriptorBuffer.buffer, &newDescriptorBuffer.allocation, &newDescriptorBuffer.info));

    newDescriptorBuffer.bufferAddress = vk_helpers::getDeviceAddress(ctx.device, newDescriptorBuffer.buffer);
    newDescriptorBuffer.m_destroyed = false;
    return newDescriptorBuffer;
}

int32_t DescriptorBufferSampler::setupData(const VulkanContext& ctx, const std::vector<DescriptorImageData>& imageBuffers,
                                           const int32_t index /*= -1*/)
{
    int descriptorBufferIndex;
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

    uint64_t accumOffset{descriptorBufferOffset};

    if (ctx.deviceDescriptorBufferProperties.combinedImageSamplerDescriptorSingleArray == VK_FALSE) {
        fmt::print("This implementation does not support combinedImageSamplerDescriptorSingleArray\n");
        return -1;
    }

    for (const DescriptorImageData currentData : imageBuffers) {
        if (currentData.bIsPadding) {
            size_t descriptorSize;
            switch (currentData.type) {
                case VK_DESCRIPTOR_TYPE_SAMPLER:
                    descriptorSize = ctx.deviceDescriptorBufferProperties.samplerDescriptorSize;
                    break;
                case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                    descriptorSize = ctx.deviceDescriptorBufferProperties.combinedImageSamplerDescriptorSize;
                    break;
                case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                    descriptorSize = ctx.deviceDescriptorBufferProperties.sampledImageDescriptorSize;
                    break;
                case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                    descriptorSize = ctx.deviceDescriptorBufferProperties.storageImageDescriptorSize;
                    break;
                default:
                    fmt::print("DescriptorBufferImage::setup_data() called with a non-image/sampler type in its data");
                    return -1;
            }
            accumOffset += descriptorSize;
            continue;
        }

        VkDescriptorGetInfoEXT image_descriptor_info{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};
        image_descriptor_info.type = currentData.type;
        image_descriptor_info.pNext = nullptr;
        size_t descriptor_size;

        switch (currentData.type) {
            case VK_DESCRIPTOR_TYPE_SAMPLER:
                image_descriptor_info.data.pSampler = &currentData.imageInfo.sampler;
                descriptor_size = ctx.deviceDescriptorBufferProperties.samplerDescriptorSize;
                break;
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                image_descriptor_info.data.pCombinedImageSampler = &currentData.imageInfo;
                descriptor_size = ctx.deviceDescriptorBufferProperties.combinedImageSamplerDescriptorSize;
                break;
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                image_descriptor_info.data.pSampledImage = &currentData.imageInfo;
                descriptor_size = ctx.deviceDescriptorBufferProperties.sampledImageDescriptorSize;
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                image_descriptor_info.data.pStorageImage = &currentData.imageInfo;
                descriptor_size = ctx.deviceDescriptorBufferProperties.storageImageDescriptorSize;
                break;
            default:
                fmt::print("DescriptorBufferImage::setup_data() called with a non-image/sampler descriptor type\n");
                return -1;
        }
        char* buffer_ptr_offset = static_cast<char*>(info.pMappedData) + accumOffset + descriptorBufferIndex *
                                  descriptorBufferSize;

        vkGetDescriptorEXT(ctx.device, &image_descriptor_info, descriptor_size, buffer_ptr_offset);

        accumOffset += descriptor_size;
    }

    return descriptorBufferIndex;
}

VkBufferUsageFlagBits DescriptorBufferSampler::getBufferUsageFlags() const
{
    return static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT);
}
}
