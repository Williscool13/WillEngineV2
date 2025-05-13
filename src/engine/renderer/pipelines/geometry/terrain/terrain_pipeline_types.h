//
// Created by William on 2025-04-14.
//

#ifndef TERRAIN_PIPELINE_TYPES_H
#define TERRAIN_PIPELINE_TYPES_H

#include <unordered_set>
#include <glm/glm.hpp>
#include <volk/volk.h>

#include "engine/renderer/renderer_constants.h"

namespace will_engine
{
class ITerrain;
}

namespace will_engine::terrain
{
struct TerrainPushConstants
{
    float tessLevel;
};

struct TerrainDrawInfo
{
    bool bClearColor{true};
    bool bDrawLinesOnly{false};
    int32_t currentFrameOverlap{0};
    glm::vec2 viewportExtents{RENDER_EXTENT_WIDTH, RENDER_EXTENT_HEIGHT};
    const std::unordered_set<ITerrain*>& terrains;
    VkImageView normalTarget{VK_NULL_HANDLE};
    VkImageView albedoTarget{VK_NULL_HANDLE};
    VkImageView pbrTarget{VK_NULL_HANDLE};
    VkImageView velocityTarget{VK_NULL_HANDLE};
    VkImageView depthTarget{VK_NULL_HANDLE};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
};
}

#endif //TERRAIN_PIPELINE_TYPES_H
