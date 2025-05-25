//
// Created by William on 2025-05-25.
//

#include "pipeline.h"

#include <volk/volk.h>

#include "engine/renderer/vk_helpers.h"

namespace will_engine::renderer
{
void Pipeline::release(VulkanContext& context)
{
    if (!m_destroyed) {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(context.device, pipeline, nullptr);
            pipeline = VK_NULL_HANDLE;
        }
        m_destroyed = true;
    }
}

bool Pipeline::isValid() const
{
    return !m_destroyed && pipeline != VK_NULL_HANDLE;
}

Pipeline Pipeline::createCompute(VkDevice device, const VkComputePipelineCreateInfo& createInfo)
{
    Pipeline newPipeline{};
    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &newPipeline.pipeline));
    newPipeline.m_destroyed = false;
    return newPipeline;
}

Pipeline Pipeline::createRender(VkDevice device, const VkGraphicsPipelineCreateInfo& createInfo)
{
    Pipeline newPipeline{};
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &newPipeline.pipeline));
    newPipeline.m_destroyed = false;
    return newPipeline;
}
}
