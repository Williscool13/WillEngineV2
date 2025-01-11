//
// Created by William on 2025-01-01.
//

#ifndef SHADOW_MAP_DESCRIPTOR_LAYOUTS_H
#define SHADOW_MAP_DESCRIPTOR_LAYOUTS_H

#include <vulkan/vulkan_core.h>
#include "src/renderer/vulkan_context.h"


class CascadedShadowMapDescriptorLayouts
{
public:
    explicit CascadedShadowMapDescriptorLayouts(VulkanContext& context) : context(context)
    {
        createLayouts();
    }

    ~CascadedShadowMapDescriptorLayouts()
    {
        cleanup();
    }

public:
    [[nodiscard]] VkDescriptorSetLayout getCascadedShadowMapUniformLayout() const { return cascadedShadowMapUniformLayout; }
    [[nodiscard]] VkDescriptorSetLayout getCascadedShadowMapSamplerLayout() const { return cascadedShadowMapSamplerLayout; }

private:
    void createLayouts();

    void cleanup() const;

    VulkanContext& context;

    VkDescriptorSetLayout cascadedShadowMapUniformLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout cascadedShadowMapSamplerLayout{VK_NULL_HANDLE};
};


#endif //SHADOW_MAP_DESCRIPTOR_LAYOUTS_H
