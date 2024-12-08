//
// Created by William on 2024-12-08.
//

#include "frustum_culling_descriptor_layout.h"

#include "src/renderer/vk_descriptors.h"
#include "src/renderer/vulkan_context.h"

void FrustumCullingDescriptorLayouts::createLayouts()
{
    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    frustumCullLayout = layoutBuilder.build(context.getDevice(), VK_SHADER_STAGE_COMPUTE_BIT
                                            , nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
}

void FrustumCullingDescriptorLayouts::cleanup() const
{
    if (context.getDevice() != VK_NULL_HANDLE) {
        if (frustumCullLayout) vkDestroyDescriptorSetLayout(context.getDevice(), frustumCullLayout, nullptr);
    }
}
