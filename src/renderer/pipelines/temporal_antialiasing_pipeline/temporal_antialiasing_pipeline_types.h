//
// Created by William on 2025-04-14.
//

#ifndef TEMPORAL_ANTIALIASING_PIPELINE_TYPES_H
#define TEMPORAL_ANTIALIASING_PIPELINE_TYPES_H

#include <volk/volk.h>

namespace will_engine::temporal_antialiasing_pipeline
{
struct TemporalAntialiasingPushConstants
{
    float blendValue{0.1f};
    int32_t taaDebug{0};
};

struct TemporalAntialiasingDescriptor
{
    VkImageView drawImage{VK_NULL_HANDLE};
    VkImageView historyBuffer{VK_NULL_HANDLE};
    VkImageView depthBuffer{VK_NULL_HANDLE};
    VkImageView velocityBuffer{VK_NULL_HANDLE};
    VkImageView outputTarget{VK_NULL_HANDLE};
    VkSampler sampler{VK_NULL_HANDLE};
};

struct TemporalAntialiasingDrawInfo
{
    float blendValue{};
    int32_t debugMode{};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
};
} // temporal_antialiasing_pipeline

#endif //TEMPORAL_ANTIALIASING_PIPELINE_TYPES_H
