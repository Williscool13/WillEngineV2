//
// Created by William on 2025-05-25.
//

#include "pipeline_layout.h"

#include <volk/volk.h>

#include "engine/renderer/resource_manager.h"
#include "engine/renderer/vk_helpers.h"

namespace will_engine::renderer
{
PipelineLayout::PipelineLayout(ResourceManager* resourceManager, const VkPipelineLayoutCreateInfo& createInfo) : VulkanResource(resourceManager)
{
    VK_CHECK(vkCreatePipelineLayout(manager->getDevice(), &createInfo, nullptr, &layout));
}

PipelineLayout::~PipelineLayout()
{
    if (layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(manager->getDevice(), layout, nullptr);
        layout = VK_NULL_HANDLE;
    }
}
}
