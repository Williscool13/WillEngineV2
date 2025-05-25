//
// Created by William on 2025-05-25.
//

#ifndef PIPELINE_H
#define PIPELINE_H
#include <cassert>
#include <utility>
#include <vulkan/vulkan_core.h>

#include "vulkan_resource.h"
#include "engine/renderer/vulkan_context.h"

namespace will_engine::renderer
{
/**
* A wrapper for Vulkan pipelines.
*/
class Pipeline : public VulkanResource
{
public:
    VkPipeline pipeline{VK_NULL_HANDLE};
    VkPipelineLayout layout{VK_NULL_HANDLE};

    Pipeline() = default;

    ~Pipeline() override
    {
        assert(m_destroyed && "Resource not properly destroyed before destructor!");
    }

    void release(VulkanContext& context) override;

    bool isValid() const override;

    static Pipeline createCompute(VkDevice device, const VkComputePipelineCreateInfo& createInfo);

    static Pipeline createRender(VkDevice device, const VkGraphicsPipelineCreateInfo& createInfo);

    // No copying
    Pipeline(const Pipeline&) = delete;

    Pipeline& operator=(const Pipeline&) = delete;

    // Move constructor
    Pipeline(Pipeline&& other) noexcept
        : pipeline(std::exchange(other.pipeline, VK_NULL_HANDLE))
          , layout(std::exchange(other.layout, VK_NULL_HANDLE))
          , m_destroyed(other.m_destroyed)
    {
        other.m_destroyed = true;
    }

    Pipeline& operator=(Pipeline&& other) noexcept
    {
        if (this != &other) {
            pipeline = std::exchange(other.pipeline, VK_NULL_HANDLE);
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

#endif //PIPELINE_H
