//
// Created by William on 2025-05-25.
//

#ifndef PIPELINE_LAYOUT_H
#define PIPELINE_LAYOUT_H

#include <vulkan/vulkan_core.h>

#include "vulkan_resource.h"

namespace will_engine::renderer
{
/**
* A wrapper for a VkPipelineLayout.
*/
struct PipelineLayout : VulkanResource
{
    VkPipelineLayout layout{VK_NULL_HANDLE};

    PipelineLayout(ResourceManager* resourceManager, const VkPipelineLayoutCreateInfo& createInfo);

    ~PipelineLayout() override;
};
}

#endif //PIPELINE_LAYOUT_H