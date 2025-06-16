//
// Created by William on 2025-05-25.
//

#ifndef PIPELINE_H
#define PIPELINE_H

#include <vulkan/vulkan_core.h>

#include "vulkan_resource.h"

namespace will_engine::renderer
{
/**
* A wrapper for Vulkan pipelines.
*/
struct Pipeline : VulkanResource
{
    VkPipeline pipeline{VK_NULL_HANDLE};

    Pipeline(ResourceManager* resourceManager, const VkComputePipelineCreateInfo& createInfo);
    Pipeline(ResourceManager* resourceManager, const VkGraphicsPipelineCreateInfo& createInfo);

    ~Pipeline() override;
};
}

#endif //PIPELINE_H