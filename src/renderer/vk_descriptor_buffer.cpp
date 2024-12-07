//
// Created by William on 2024-08-17.
//

#include "vk_descriptor_buffer.h"

VkPhysicalDeviceDescriptorBufferPropertiesEXT DescriptorBuffer::deviceDescriptorBufferProperties = {};
bool DescriptorBuffer::devicePropertiesRetrieved = false;

class DescriptorBufferException : public std::runtime_error
{
public:
    explicit DescriptorBufferException(const std::string& message) : std::runtime_error(message) {}
};

DescriptorBuffer::DescriptorBuffer(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, VmaAllocator allocator,
                                   VkDescriptorSetLayout descriptorSetLayout, int maxObjectCount)
{
    if (!devicePropertiesRetrieved) {
        VkPhysicalDeviceProperties2KHR device_properties{};
        deviceDescriptorBufferProperties = {};
        deviceDescriptorBufferProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;
        device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
        device_properties.pNext = &deviceDescriptorBufferProperties;
        vkGetPhysicalDeviceProperties2(physicalDevice, &device_properties);
        devicePropertiesRetrieved = true;
    }

    this->descriptorSetLayout = descriptorSetLayout;


    // Get size per Descriptor Set
    vkGetDescriptorSetLayoutSizeEXT(device, descriptorSetLayout, &descriptorBufferSize);
    descriptorBufferSize = vk_helpers::getAlignedSize(descriptorBufferSize, deviceDescriptorBufferProperties.descriptorBufferOffsetAlignment);
    // Get Descriptor Buffer offset
    vkGetDescriptorSetLayoutBindingOffsetEXT(device, descriptorSetLayout, 0u, &descriptorBufferOffset);

    freeIndices = std::unordered_set<int>();
    for (int i = 0; i < maxObjectCount; i++) { freeIndices.insert(i); }

    this->maxObjectCount = maxObjectCount;
}

void DescriptorBuffer::destroy(VkDevice device, VmaAllocator allocator)
{
    //TODO: Maybe need to loop through all active indices and free those resources too? idk
    vmaDestroyBuffer(allocator, descriptorBuffer.buffer, descriptorBuffer.allocation);
}

void DescriptorBuffer::freeDescriptorBuffer(int index)
{
    freeIndices.insert(index);
}

VkDescriptorBufferBindingInfoEXT DescriptorBuffer::getDescriptorBufferBindingInfo() const
{
    VkDescriptorBufferBindingInfoEXT descriptor_buffer_binding_info{};
    descriptor_buffer_binding_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
    descriptor_buffer_binding_info.address = descriptorBufferGpuAddress;
    descriptor_buffer_binding_info.usage = getBufferUsageFlags();

    return descriptor_buffer_binding_info;
}

DescriptorBufferUniform::DescriptorBufferUniform(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, VmaAllocator allocator,
                                                 VkDescriptorSetLayout descriptorSetLayout, int maxObjectCount)
    : DescriptorBuffer(instance, device, physicalDevice, allocator, descriptorSetLayout, maxObjectCount)
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
    VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &vmaAllocInfo, &descriptorBuffer.buffer, &descriptorBuffer.allocation, &descriptorBuffer.info));

    descriptorBufferGpuAddress = vk_helpers::getDeviceAddress(device, descriptorBuffer.buffer);
}

int DescriptorBufferUniform::setupData(VkDevice device, std::vector<DescriptorUniformData>& uniformBuffers)
{
    // TODO: Manage what happens if attempt to allocate a descriptor buffer set but out of space
    if (freeIndices.empty()) {
        throw DescriptorBufferException("Ran out of space in DescriptorBufferUniform");
    }

    const int descriptorBufferIndex = *freeIndices.begin();
    freeIndices.erase(freeIndices.begin());


    uint64_t accum_offset{descriptorBufferOffset};

    for (int i = 0; i < uniformBuffers.size(); i++) {
        VkDeviceAddress uniformBufferAddress = vk_helpers::getDeviceAddress(device, uniformBuffers[i].uniformBuffer.buffer);


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
        char* buffer_ptr_offset = static_cast<char*>(descriptorBuffer.info.pMappedData) + accum_offset + descriptorBufferIndex *
                                  descriptorBufferSize;

        vkGetDescriptorEXT(
            device
            , &descriptorGetInfo
            , deviceDescriptorBufferProperties.uniformBufferDescriptorSize
            , buffer_ptr_offset
        );

        accum_offset += deviceDescriptorBufferProperties.uniformBufferDescriptorSize;
    }


    return descriptorBufferIndex;
}

VkBufferUsageFlagBits DescriptorBufferUniform::getBufferUsageFlags() const
{
    return VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;
}

DescriptorBufferSampler::DescriptorBufferSampler(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, VmaAllocator allocator,
                                                 VkDescriptorSetLayout descriptorSetLayout, int maxObjectCount)
    : DescriptorBuffer(instance, device, physicalDevice, allocator, descriptorSetLayout, maxObjectCount)
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
    VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &vmaAllocInfo, &descriptorBuffer.buffer, &descriptorBuffer.allocation, &descriptorBuffer.info));

    descriptorBufferGpuAddress = vk_helpers::getDeviceAddress(device, descriptorBuffer.buffer);
}

int DescriptorBufferSampler::setupData(VkDevice device, const std::vector<DescriptorImageData>& imageBuffers, const int index /*= -1*/)
{
    int descriptorBufferIndex{};
    if (index < 0) {
        // TODO: Manage what happens if attempt to allocate a descriptor buffer set but out of space
        if (freeIndices.empty()) {
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
        size_t descriptor_size{};

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

        vkGetDescriptorEXT(
            device,
            &image_descriptor_info,
            descriptor_size,
            buffer_ptr_offset
        );

        accumOffset += descriptor_size;
    }

    return descriptorBufferIndex;
}

VkBufferUsageFlagBits DescriptorBufferSampler::getBufferUsageFlags() const
{
    return static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT);
}
