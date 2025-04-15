//
// Created by William on 2025-04-14.
//

#ifndef BASIC_RENDER_PIPELINE_TYPES_H
#define BASIC_RENDER_PIPELINE_TYPES_H

#include <volk/volk.h>

namespace will_engine::basic_render_pipeline
{
struct RenderDescriptorInfo
{
    VkSampler sampler{VK_NULL_HANDLE};
    VkImageView texture{VK_NULL_HANDLE};
};

struct RenderDrawInfo
{
    VkExtent2D renderExtent{};
    VkImageView drawImage{VK_NULL_HANDLE};
    VkImageView depthImage{VK_NULL_HANDLE};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
    int32_t currentFrame{};
};

struct RenderPushConstant
{
    int32_t currentFrame;
};
}

#endif //BASIC_RENDER_PIPELINE_TYPES_H
