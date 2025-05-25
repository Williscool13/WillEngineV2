//
// Created by William on 2025-05-25.
//

#include "descriptor_set_layout.h"

#include <volk/volk.h>

#include "engine/renderer/vk_helpers.h"

namespace will_engine::renderer
{
void DescriptorSetLayout::release(VulkanContext& context)
{
    if (!m_destroyed) {
        if (layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(context.device, layout, nullptr);
            layout = VK_NULL_HANDLE;
        }
        m_destroyed = true;
    }
}
bool DescriptorSetLayout::isValid() const
{
    return !m_destroyed && layout != VK_NULL_HANDLE;
}

DescriptorSetLayout DescriptorSetLayout::create(VkDevice device, const VkDescriptorSetLayoutCreateInfo& createInfo)
{
    DescriptorSetLayout newSetLayout{};
    VK_CHECK(vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &newSetLayout.layout));
    newSetLayout.m_destroyed = false;
    return newSetLayout;
}
}
