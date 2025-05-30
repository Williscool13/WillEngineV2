//
// Created by William on 2024-08-17.
//

#include "descriptor_buffer.h"

#include "engine/renderer/resource_manager.h"
#include "engine/renderer/vk_helpers.h"
#include "engine/renderer/vulkan_context.h"

namespace will_engine::renderer
{
DescriptorBuffer::DescriptorBuffer(ResourceManager* resourceManager) : VulkanResource(resourceManager)
{}

DescriptorBuffer::~DescriptorBuffer()
{
    if (buffer != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE) {
        vmaDestroyBuffer(manager->getAllocator(), buffer, allocation);
        buffer = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
    }
    info = {};
}

void DescriptorBuffer::freeDescriptorBufferIndex(int32_t index)
{
    freeIndices.insert(index);
}

void DescriptorBuffer::freeAllDescriptorBufferIndices()
{
    for (int32_t i = 0; i < maxObjectCount; i++) { freeIndices.insert(i); }
}

VkDescriptorBufferBindingInfoEXT DescriptorBuffer::getBindingInfo() const
{
    VkDescriptorBufferBindingInfoEXT descriptor_buffer_binding_info{};
    descriptor_buffer_binding_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
    descriptor_buffer_binding_info.address = bufferAddress;
    descriptor_buffer_binding_info.usage = getBufferUsageFlags();

    return descriptor_buffer_binding_info;
}
}
