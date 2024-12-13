//
// Created by William on 2024-12-08.
//

#include "render_object_descriptor_layout.h"

#include "src/renderer/vk_descriptors.h"
#include "src/renderer/vulkan_context.h"
#include "src/renderer/render_object/render_object_constants.h"

void RenderObjectDescriptorLayout::createLayouts()
{
    {
        // Render binding 0
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        addressesLayout = layoutBuilder.build(context.device,
                                                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
                                                           , nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    } {
        // Render binding 1
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_SAMPLER, render_object_constants::MAX_SAMPLER_COUNT);
        layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, render_object_constants::MAX_IMAGES_COUNT);

        texturesLayout = layoutBuilder.build(context.device, VK_SHADER_STAGE_FRAGMENT_BIT
                                                         , nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }
}

void RenderObjectDescriptorLayout::cleanup() const
{
    if (context.device != VK_NULL_HANDLE) {
        if (addressesLayout) vkDestroyDescriptorSetLayout(context.device, addressesLayout, nullptr);
        if (texturesLayout) vkDestroyDescriptorSetLayout(context.device, texturesLayout, nullptr);
    }
}
