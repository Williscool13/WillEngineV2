//
// Created by William on 2025-04-14.
//

#ifndef ENVIRONMENT_PIPELINE_TYPES_H
#define ENVIRONMENT_PIPELINE_TYPES_H

#include <volk/volk.h>

namespace will_engine::environment_pipeline
{
struct EnvironmentDrawInfo
{
    bool bClearColor{false};
    VkImageView normalTarget{VK_NULL_HANDLE};
    VkImageView albedoTarget{VK_NULL_HANDLE};
    VkImageView pbrTarget{VK_NULL_HANDLE};
    VkImageView velocityTarget{VK_NULL_HANDLE};
    VkImageView depthTarget{VK_NULL_HANDLE};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{};
    VkDescriptorBufferBindingInfoEXT environmentMapBinding{};
    VkDeviceSize environmentMapOffset{};
};
}

#endif //ENVIRONMENT_PIPELINE_TYPES_H
