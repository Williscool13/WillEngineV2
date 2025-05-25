//
// Created by William on 2025-05-25.
//

#ifndef DESCRIPTOR_SET_LAYOUT_H
#define DESCRIPTOR_SET_LAYOUT_H
#include <cassert>
#include <utility>
#include <vulkan/vulkan_core.h>

#include "vulkan_resource.h"
#include "engine/renderer/vulkan_context.h"

namespace will_engine::renderer
{
/**
* A wrapper for a VkDescriptorSetLayout.
*/
class DescriptorSetLayout : public VulkanResource
{
public:
    VkDescriptorSetLayout layout{VK_NULL_HANDLE};

    DescriptorSetLayout() = default;


    ~DescriptorSetLayout() override
    {
        assert(m_destroyed && "Resource not properly destroyed before destructor!");
    }

    void release(VulkanContext& context) override;

    bool isValid() const override;

    static DescriptorSetLayout create(VkDevice device, const VkDescriptorSetLayoutCreateInfo& createInfo);

    // No copying
    DescriptorSetLayout(const DescriptorSetLayout&) = delete;
    DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

    // Move constructor
    DescriptorSetLayout(DescriptorSetLayout&& other) noexcept
        : layout(std::exchange(other.layout, VK_NULL_HANDLE))
          , m_destroyed(other.m_destroyed)
    {
        other.m_destroyed = true;
    }

    DescriptorSetLayout& operator=(DescriptorSetLayout&& other) noexcept
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

#endif //DESCRIPTOR_SET_LAYOUT_H