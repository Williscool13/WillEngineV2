//
// Created by William on 2024-12-08.
//

#include "scene_descriptor_layouts.h"

#include <vulkan/vulkan_core.h>
#include "src/renderer/vk_descriptors.h"
#include "src/renderer/vulkan_context.h"

void SceneDescriptorLayouts::createLayouts()
{
    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    sceneDataLayout = layoutBuilder.build(context.device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, nullptr,
                                          VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
}

void SceneDescriptorLayouts::cleanup() const
{
    if (context.device != VK_NULL_HANDLE) {
        if (sceneDataLayout) vkDestroyDescriptorSetLayout(context.device, sceneDataLayout, nullptr);
    }
}
