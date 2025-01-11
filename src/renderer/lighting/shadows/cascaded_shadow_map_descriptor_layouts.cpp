//
// Created by William on 2025-01-01.
//

#include "cascaded_shadow_map_descriptor_layouts.h"

#include <volk.h>

#include "cascaded_shadow_map.h"
#include "src/renderer/vk_descriptors.h"

void CascadedShadowMapDescriptorLayouts::createLayouts()
{
    // (Uniform) Shadow Map Layout - Used by deferred resolve
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        cascadedShadowMapUniformLayout = layoutBuilder.build(context.device, VK_SHADER_STAGE_COMPUTE_BIT, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }

    // (Sampler) Shadow Map Layout - Used by deferred resolve
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, CascadedShadowMap::SHADOW_CASCADE_SPLIT_COUNT);
        cascadedShadowMapSamplerLayout = layoutBuilder.build(context.device, VK_SHADER_STAGE_COMPUTE_BIT, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }
}

void CascadedShadowMapDescriptorLayouts::cleanup() const
{
    if (context.device != VK_NULL_HANDLE) {
        if (cascadedShadowMapSamplerLayout) vkDestroyDescriptorSetLayout(context.device, cascadedShadowMapSamplerLayout, nullptr);
    }
    if (context.device != VK_NULL_HANDLE) {
        if (cascadedShadowMapUniformLayout) vkDestroyDescriptorSetLayout(context.device, cascadedShadowMapUniformLayout, nullptr);
    }
}
