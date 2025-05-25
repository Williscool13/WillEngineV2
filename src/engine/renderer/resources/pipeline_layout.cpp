//
// Created by William on 2025-05-25.
//

#include "pipeline_layout.h"

#include <volk/volk.h>

#include "engine/renderer/vk_helpers.h"

namespace will_engine::renderer
{
void PipelineLayout::release(VulkanContext& context)
{
    if (!m_destroyed) {
        if (layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(context.device, layout, nullptr);
            layout = VK_NULL_HANDLE;
        }
        m_destroyed = true;
    }
}

bool PipelineLayout::isValid() const
{
    return !m_destroyed && layout != VK_NULL_HANDLE;
}
PipelineLayout PipelineLayout::create(VkDevice device, const VkPipelineLayoutCreateInfo& createInfo)
{
    PipelineLayout newPipelineLayout{};
    VK_CHECK(vkCreatePipelineLayout(device, &createInfo, nullptr, &newPipelineLayout.layout));
    newPipelineLayout.m_destroyed = false;
    return newPipelineLayout;
}
}
