//
// Created by William on 2025-05-25.
//

#ifndef PIPELINE_LAYOUT_H
#define PIPELINE_LAYOUT_H
#include <cassert>
#include <utility>
#include <vulkan/vulkan_core.h>

#include "vulkan_resource.h"
#include "engine/renderer/vulkan_context.h"

namespace will_engine::renderer
{
/**
 * A wrapper for a VkPipelineLayout.
 */
class PipelineLayout : public VulkanResource
{
public:
    VkPipelineLayout layout{VK_NULL_HANDLE};

    PipelineLayout() = default;

    ~PipelineLayout() override
    {
        assert(m_destroyed && "Resource not properly destroyed before destructor!");
    }

    void release(VulkanContext& context) override;

    bool isValid() const override;

    static PipelineLayout create(VkDevice device, const VkPipelineLayoutCreateInfo& createInfo);

    // No copying
    PipelineLayout(const PipelineLayout&) = delete;

    PipelineLayout& operator=(const PipelineLayout&) = delete;

    // Move constructor
    PipelineLayout(PipelineLayout&& other) noexcept
        : layout(std::exchange(other.layout, VK_NULL_HANDLE))
          , m_destroyed(other.m_destroyed)
    {
        other.m_destroyed = true;
    }

    PipelineLayout& operator=(PipelineLayout&& other) noexcept
    {
        if (this != &other) {
            layout = std::exchange(other.layout, VK_NULL_HANDLE);
            m_destroyed = other.m_destroyed;
            other.m_destroyed = true;
        }
        return *this;
    }

private:
    bool m_destroyed{true};
};
}

#endif //PIPELINE_LAYOUT_H
