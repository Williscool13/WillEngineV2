//
// Created by William on 2025-05-25.
//

#include "sampler.h"

#include <volk/volk.h>

namespace will_engine::renderer
{
void Sampler::release(VulkanContext& context)
{
    if (!m_destroyed) {
        if (sampler != VK_NULL_HANDLE) {
            vkDestroySampler(context.device, sampler, nullptr);
            sampler = VK_NULL_HANDLE;
        }
        m_destroyed = true;
    }
}

bool Sampler::isValid() const
{
    return !m_destroyed && sampler != VK_NULL_HANDLE;
}

Sampler Sampler::create(VkDevice device, const VkSamplerCreateInfo& createInfo)
{
    Sampler newSampler{};
    vkCreateSampler(device, &createInfo, nullptr, &newSampler.sampler);
    newSampler.m_destroyed = false;
    return newSampler;
}
}
