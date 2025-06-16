//
// Created by William on 8/11/2024.
//

#include "vk_descriptors.h"

#include <volk/volk.h>


#include "vk_helpers.h"

DescriptorLayoutBuilder::DescriptorLayoutBuilder(const uint32_t reservedSize)
{
    if (reservedSize > 0) {
        bindings.reserve(reservedSize);
    }
}

void DescriptorLayoutBuilder::addBinding(uint32_t binding, VkDescriptorType type)
{
    VkDescriptorSetLayoutBinding newbind{};
    newbind.binding = binding;
    newbind.descriptorCount = 1;
    newbind.descriptorType = type;

    bindings.push_back(newbind);
}

void DescriptorLayoutBuilder::addBinding(uint32_t binding, VkDescriptorType type, uint32_t count)
{
    VkDescriptorSetLayoutBinding newbind{};
    newbind.binding = binding;
    newbind.descriptorCount = count;
    newbind.descriptorType = type;

    bindings.push_back(newbind);
}

void DescriptorLayoutBuilder::clear()
{
    bindings.clear();
}

VkDescriptorSetLayoutCreateInfo DescriptorLayoutBuilder::build(const VkShaderStageFlagBits shaderStageFlags,
                                                               const VkDescriptorSetLayoutCreateFlags layoutCreateFlags)
{
    for (auto& b : bindings) {
        b.stageFlags |= shaderStageFlags;
    }

    return {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = layoutCreateFlags,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };
}


VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext,
                                                     VkDescriptorSetLayoutCreateFlags flags)
{
    for (auto& b : bindings) {
        b.stageFlags |= shaderStages;
    }

    VkDescriptorSetLayoutCreateInfo info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    info.pNext = pNext;

    info.pBindings = bindings.data();
    info.bindingCount = (uint32_t) bindings.size();
    info.flags = flags;

    VkDescriptorSetLayout set;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

    return set;
}
