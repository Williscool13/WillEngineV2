//
// Created by William on 2025-05-25.
//

#include "descriptor_set_layout.h"

#include <volk/volk.h>

#include "engine/renderer/vk_helpers.h"
#include "engine/renderer/resource_manager.h"

namespace will_engine::renderer
{
DescriptorSetLayout::DescriptorSetLayout(ResourceManager* resourceManager, const VkDescriptorSetLayoutCreateInfo& createInfo)
    : VulkanResource(resourceManager)
{
    VK_CHECK(vkCreateDescriptorSetLayout(resourceManager->getDevice(), &createInfo, nullptr, &layout));
}

DescriptorSetLayout::~DescriptorSetLayout()
{
    if (layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(manager->getDevice(), layout, nullptr);
        layout = VK_NULL_HANDLE;
    }
}
}
