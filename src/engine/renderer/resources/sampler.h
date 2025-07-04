//
// Created by William on 2025-05-25.
//

#ifndef SAMPLER_H
#define SAMPLER_H

#include <vulkan/vulkan_core.h>

#include "vulkan_resource.h"

namespace will_engine::renderer
{
struct Sampler : VulkanResource
{
    VkSampler sampler{VK_NULL_HANDLE};

    Sampler(ResourceManager* resourceManager, const VkSamplerCreateInfo& createInfo);

    ~Sampler() override;
};
}

#endif //SAMPLER_H
