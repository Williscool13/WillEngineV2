//
// Created by William on 2025-01-01.
//

#ifndef SHADOW_MAP_DESCRIPTOR_LAYOUTS_H
#define SHADOW_MAP_DESCRIPTOR_LAYOUTS_H

#include <vulkan/vulkan_core.h>
#include "src/renderer/vulkan_context.h"


class ShadowMapDescriptorLayouts
{
public:
    explicit ShadowMapDescriptorLayouts(VulkanContext& context) : context(context)
    {
        createLayouts();
    }

    ~ShadowMapDescriptorLayouts()
    {
        cleanup();
    }

public:
    [[nodiscard]] VkDescriptorSetLayout getShadowMapLayout() const { return shadowMapLayout; }

private:
    void createLayouts();

    void cleanup() const;

    VulkanContext& context;

    VkDescriptorSetLayout shadowMapLayout{VK_NULL_HANDLE};
};


#endif //SHADOW_MAP_DESCRIPTOR_LAYOUTS_H
