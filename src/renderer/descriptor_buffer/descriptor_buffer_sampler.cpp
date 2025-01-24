//
// Created by William on 2025-01-22.
//

#include "descriptor_buffer_sampler.h"

#include "volk.h"
#include "src/renderer/vk_helpers.h"
#include "src/renderer/vulkan_context.h"

using will_engine::DescriptorImageData;
using will_engine::DescriptorBufferException;

DescriptorBufferSampler::DescriptorBufferSampler(const VulkanContext& context, VkDescriptorSetLayout descriptorSetLayout, const int maxObjectCount)
    : DescriptorBuffer(context, descriptorSetLayout, maxObjectCount)
{
    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = descriptorBufferSize * maxObjectCount;
    bufferInfo.usage =
            VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
            | VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;
    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VK_CHECK(vmaCreateBuffer(context.allocator, &bufferInfo, &vmaAllocInfo, &descriptorBuffer.buffer, &descriptorBuffer.allocation, &descriptorBuffer.info));

    descriptorBufferGpuAddress = vk_helpers::getDeviceAddress(context.device, descriptorBuffer.buffer);
}

int DescriptorBufferSampler::setupData(VkDevice device, const std::vector<DescriptorImageData>& imageBuffers, const int index /*= -1*/)
{
    int descriptorBufferIndex;
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

    uint64_t accumOffset{descriptorBufferOffset};

    if (deviceDescriptorBufferProperties.combinedImageSamplerDescriptorSingleArray == VK_FALSE) {
        fmt::print("This implementation does not support combinedImageSamplerDescriptorSingleArray\n");
        return -1;
    }

    for (const DescriptorImageData currentData : imageBuffers) {
        if (currentData.bIsPadding) {
            size_t descriptorSize;
            switch (currentData.type) {
                case VK_DESCRIPTOR_TYPE_SAMPLER:
                    descriptorSize = deviceDescriptorBufferProperties.samplerDescriptorSize;
                    break;
                case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                    descriptorSize = deviceDescriptorBufferProperties.combinedImageSamplerDescriptorSize;
                    break;
                case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                    descriptorSize = deviceDescriptorBufferProperties.sampledImageDescriptorSize;
                    break;
                case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                    descriptorSize = deviceDescriptorBufferProperties.storageImageDescriptorSize;
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
                descriptor_size = deviceDescriptorBufferProperties.samplerDescriptorSize;
                break;
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                image_descriptor_info.data.pCombinedImageSampler = &currentData.imageInfo;
                descriptor_size = deviceDescriptorBufferProperties.combinedImageSamplerDescriptorSize;
                break;
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                image_descriptor_info.data.pSampledImage = &currentData.imageInfo;
                descriptor_size = deviceDescriptorBufferProperties.sampledImageDescriptorSize;
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                image_descriptor_info.data.pStorageImage = &currentData.imageInfo;
                descriptor_size = deviceDescriptorBufferProperties.storageImageDescriptorSize;
                break;
            default:
                fmt::print("DescriptorBufferImage::setup_data() called with a non-image/sampler descriptor type\n");
                return -1;
        }
        char* buffer_ptr_offset = static_cast<char*>(descriptorBuffer.info.pMappedData) + accumOffset + descriptorBufferIndex *
                                  descriptorBufferSize;

        vkGetDescriptorEXT(device, &image_descriptor_info, descriptor_size, buffer_ptr_offset);

        accumOffset += descriptor_size;
    }

    return descriptorBufferIndex;
}

VkBufferUsageFlagBits DescriptorBufferSampler::getBufferUsageFlags() const
{
    return static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT);
}
