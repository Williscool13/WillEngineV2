//
// Created by William on 2025-05-25.
//

#ifndef SAMPLER_H
#define SAMPLER_H
#include <cassert>
#include <utility>
#include <vulkan/vulkan_core.h>

#include "vulkan_resource.h"
#include "engine/renderer/vulkan_context.h"

namespace will_engine::renderer
{
/**
* A wrapper for a VkSampler.
*/
class Sampler : public VulkanResource
{
public:
    VkSampler sampler{VK_NULL_HANDLE};

    Sampler() = default;

    ~Sampler() override
    {
        assert(m_destroyed && "Resource not properly destroyed before destructor!");
    }

    void release(VulkanContext& context) override;

    bool isValid() const override;

    static Sampler create(VkDevice device, const VkSamplerCreateInfo& createInfo);


    // No copying
    Sampler(const Sampler&) = delete;

    Sampler& operator=(const Sampler&) = delete;

    Sampler(Sampler&& other) noexcept
        : sampler(std::exchange(other.sampler, VK_NULL_HANDLE))
          , m_destroyed(other.m_destroyed)
    {
        other.m_destroyed = true;
    }

    Sampler& operator=(Sampler&& other) noexcept
    {
        if (this != &other) {
            sampler = std::exchange(other.sampler, VK_NULL_HANDLE);
            m_destroyed = other.m_destroyed;
            other.m_destroyed = true;
        }
        return *this;
    }

private:
    bool m_destroyed{true};
};
}

#endif //SAMPLER_H
