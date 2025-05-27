//
// Created by William on 2025-05-25.
//

#ifndef DESCRIPTOR_SET_LAYOUT_H
#define DESCRIPTOR_SET_LAYOUT_H

#include <vulkan/vulkan_core.h>
#include "vulkan_resource.h"

namespace will_engine::renderer
{
class DescriptorSetLayout final : public VulkanResource
{
public:
    VkDescriptorSetLayout layout{VK_NULL_HANDLE};

    DescriptorSetLayout(ResourceManager* resourceManager, const VkDescriptorSetLayoutCreateInfo& createInfo);

    ~DescriptorSetLayout() override;
};
}

#endif //DESCRIPTOR_SET_LAYOUT_H
