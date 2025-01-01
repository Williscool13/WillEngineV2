//
// Created by William on 2025-01-01.
//

#include "shadow_map_descriptor_layouts.h"

#include "shadow_map.h"
#include "src/renderer/vk_descriptors.h"

void ShadowMapDescriptorLayouts::createLayouts()
{
    // (Sampler) Shadow Map Layout - Used by deferred resolve
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_SHADOW_CASCADES);
        shadowMapLayout = layoutBuilder.build(context.device, VK_SHADER_STAGE_COMPUTE_BIT, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }
}
void ShadowMapDescriptorLayouts::cleanup() const
{
    if (context.device != VK_NULL_HANDLE) {
        if (shadowMapLayout) vkDestroyDescriptorSetLayout(context.device, shadowMapLayout, nullptr);
    }
}
