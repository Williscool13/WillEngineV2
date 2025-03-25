//
// Created by William on 2025-03-23.
//

#ifndef AMBIENT_OCCLUSION_TYPES_H
#define AMBIENT_OCCLUSION_TYPES_H
#include <glm/glm.hpp>

#include "src/core/camera/camera.h"

namespace will_engine::ambient_occlusion
{
static constexpr int32_t DEPTH_PREFILTER_MIP_COUNT = 5;

struct GTAOPushConstants
{
    // Depth prefilter parameters
    float depthLinearizeMult;
    float depthLinearizeAdd;

    // Defaults follow Intel's implementation
    float radius = 0.5f;
    float falloff = 0.615f;
    float radiusMultiplier = 1.457f;

    glm::vec2 ndcToViewMult;
    glm::vec2 ndcToViewAdd;

    // AO parameters
    float strength;


    // Sampling parameters
    uint32_t numDirections;
    uint32_t numSteps;

    // Temporal/filter parameters
    float temporalWeight;
    float spatialFilterRadius;
};

struct GTAODrawInfo
{
    Camera* camera{nullptr};
    GTAOPushConstants pushConstants{};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
};
}

#endif //AMBIENT_OCCLUSION_TYPES_H
