//
// Created by William on 2024-08-17.
//

#include "descriptor_buffer.h"

#include "volk/volk.h"
#include "src/renderer/vk_helpers.h"
#include "src/renderer/vulkan_context.h"


VkPhysicalDeviceDescriptorBufferPropertiesEXT DescriptorBuffer::deviceDescriptorBufferProperties = {};
bool DescriptorBuffer::devicePropertiesRetrieved = false;

DescriptorBuffer::DescriptorBuffer(const VulkanContext& context, VkDescriptorSetLayout descriptorSetLayout, int32_t maxObjectCount)
{
    if (!devicePropertiesRetrieved) {
        VkPhysicalDeviceProperties2KHR device_properties{};
        deviceDescriptorBufferProperties = {};
        deviceDescriptorBufferProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;
        device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
        device_properties.pNext = &deviceDescriptorBufferProperties;
        vkGetPhysicalDeviceProperties2(context.physicalDevice, &device_properties);
        devicePropertiesRetrieved = true;
    }

    this->descriptorSetLayout = descriptorSetLayout;


    // Get size per Descriptor Set
    vkGetDescriptorSetLayoutSizeEXT(context.device, descriptorSetLayout, &descriptorBufferSize);
    descriptorBufferSize = vk_helpers::getAlignedSize(descriptorBufferSize, deviceDescriptorBufferProperties.descriptorBufferOffsetAlignment);
    // Get Descriptor Buffer offset
    vkGetDescriptorSetLayoutBindingOffsetEXT(context.device, descriptorSetLayout, 0u, &descriptorBufferOffset);

    freeIndices = std::unordered_set<int32_t>();
    for (int32_t i = 0; i < maxObjectCount; i++) { freeIndices.insert(i); }

    this->maxObjectCount = maxObjectCount;
}

void DescriptorBuffer::destroy(const VmaAllocator allocator)
{
    if (descriptorBuffer.buffer == VK_NULL_HANDLE) { return; }
    vmaDestroyBuffer(allocator, descriptorBuffer.buffer, descriptorBuffer.allocation);
    descriptorBuffer = {VK_NULL_HANDLE};
}

void DescriptorBuffer::freeDescriptorBufferIndex(const int32_t index)
{
    freeIndices.insert(index);
}

void DescriptorBuffer::freeAllDescriptorBufferIndices()
{
    for (int32_t i = 0; i < maxObjectCount; i++) { freeIndices.insert(i); }
}

VkDescriptorBufferBindingInfoEXT DescriptorBuffer::getDescriptorBufferBindingInfo() const
{
    VkDescriptorBufferBindingInfoEXT descriptor_buffer_binding_info{};
    descriptor_buffer_binding_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
    descriptor_buffer_binding_info.address = descriptorBufferGpuAddress;
    descriptor_buffer_binding_info.usage = getBufferUsageFlags();

    return descriptor_buffer_binding_info;
}
