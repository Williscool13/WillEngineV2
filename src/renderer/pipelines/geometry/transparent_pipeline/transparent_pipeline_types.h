//
// Created by William on 2025-04-14.
//

#ifndef TRANSPARENT_PIPELINE_TYPES_H
#define TRANSPARENT_PIPELINE_TYPES_H

#include <vector>

#include "volk/volk.h"

namespace will_engine
{
class RenderObject;
}

namespace will_engine::transparent_pipeline
{
struct TransparentsPushConstants
{
    int32_t bEnabled{true};
    int32_t bReceivesShadows{true};
};

struct TransparentAccumulateDrawInfo
{
    bool enabled{true};
    VkImageView depthTarget{VK_NULL_HANDLE};
    int32_t currentFrameOverlap{0};
    const std::vector<RenderObject*>& renderObjects{};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
    VkDescriptorBufferBindingInfoEXT environmentIBLBinding{};
    VkDeviceSize environmentIBLOffset{0};
    VkDescriptorBufferBindingInfoEXT cascadeUniformBinding{};
    VkDeviceSize cascadeUniformOffset{0};
    VkDescriptorBufferBindingInfoEXT cascadeSamplerBinding{};
};

struct TransparentCompositeDrawInfo
{
    VkImageView opaqueImage;
};
}

#endif //TRANSPARENT_PIPELINE_TYPES_H
