//
// Created by William on 2025-05-25.
//

#include "pipeline.h"

#include <volk/volk.h>

#include "engine/renderer/resource_manager.h"

namespace will_engine::renderer
{
Pipeline::Pipeline(ResourceManager* resourceManager, const VkComputePipelineCreateInfo& createInfo) : VulkanResource(resourceManager)
{
    VK_CHECK(vkCreateComputePipelines(resourceManager->getDevice(), VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline));
}

Pipeline::Pipeline(ResourceManager* resourceManager, const VkGraphicsPipelineCreateInfo& createInfo) : VulkanResource(resourceManager)
{
    VK_CHECK(vkCreateGraphicsPipelines(resourceManager->getDevice(), VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline));
}

Pipeline::~Pipeline()
{
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(manager->getDevice(), pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
}
}
