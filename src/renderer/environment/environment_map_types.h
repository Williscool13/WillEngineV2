//
// Created by William on 2025-01-06.
//

#ifndef ENVIRONMENT_MAP_TYPES_H
#define ENVIRONMENT_MAP_TYPES_H

#include <vulkan/vulkan_core.h>

#include "src/renderer/vk_descriptor_buffer.h"

struct EnvironmentPipelineCreateInfo
{
    VkDescriptorSetLayout sceneDataLayout;
    VkDescriptorSetLayout environmentMapLayout;
    VkFormat colorFormat;
    VkFormat depthFormat;
};

struct EnvironmentDrawInfo
{
    VkExtent2D renderExtent;
    VkImageView colorAttachment;
    VkImageView depthAttachment;
    const DescriptorBufferUniform& sceneData;
    const VkDeviceSize sceneDataOffset;
    const DescriptorBufferSampler& environmentMapData;
    const VkDeviceSize environmentMapOffset;
};

#endif //ENVIRONMENT_MAP_TYPES_H
