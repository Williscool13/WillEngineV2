//
// Created by William on 2025-05-25.
//

#include "sampler.h"

#include <volk/volk.h>

#include "engine/renderer/resource_manager.h"

namespace will_engine::renderer
{
Sampler::Sampler(ResourceManager* resourceManager, const VkSamplerCreateInfo& createInfo): VulkanResource(resourceManager)
{
    vkCreateSampler(manager->getDevice(), &createInfo, nullptr, &sampler);
}

Sampler::~Sampler()
{
    if (sampler != VK_NULL_HANDLE) {
        vkDestroySampler(manager->getDevice(), sampler, nullptr);
    }
}
}
