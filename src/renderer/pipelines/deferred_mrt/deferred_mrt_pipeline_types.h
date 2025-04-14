//
// Created by William on 2025-04-14.
//

#ifndef DEFERRED_MRT_TYPES_H
#define DEFERRED_MRT_TYPES_H

#include <glm/glm.hpp>

#include "src/renderer/renderer_constants.h"

namespace will_engine::deferred_mrt
{
struct DeferredMrtDrawInfo
{
    bool bClearColor{true};
    int32_t currentFrameOverlap{0};
    glm::vec2 viewportExtents{RENDER_EXTENT_WIDTH, RENDER_EXTENT_HEIGHT};
    const std::vector<RenderObject*>& renderObjects{};
    VkImageView normalTarget{VK_NULL_HANDLE};
    VkImageView albedoTarget{VK_NULL_HANDLE};
    VkImageView pbrTarget{VK_NULL_HANDLE};
    VkImageView velocityTarget{VK_NULL_HANDLE};
    VkImageView depthTarget{VK_NULL_HANDLE};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
};
} // deferred_mrt::will_engine

#endif //DEFERRED_MRT_TYPES_H
