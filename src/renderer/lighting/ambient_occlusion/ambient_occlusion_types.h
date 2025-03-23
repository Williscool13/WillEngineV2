//
// Created by William on 2025-03-23.
//

#ifndef AMBIENT_OCCLUSION_TYPES_H
#define AMBIENT_OCCLUSION_TYPES_H
#include <glm/glm.hpp>

namespace will_engine::ambient_occlusion
{
static constexpr int32_t DEPTH_PREFILTER_MIP_COUNT = 5;

struct GTAOPushConstants
{
    glm::vec2 viewportSize;
    glm::vec2 viewportPixelSize;

    // AO parameters
    float radius;
    float falloff;
    float strength;
    float radiusMultiplier;

    // Sampling parameters
    uint32_t numDirections;
    uint32_t numSteps;

    // Temporal/filter parameters
    float temporalWeight;
    float spatialFilterRadius;
};

struct GTAODrawInfo
{
    GTAOPushConstants pushConstants{};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
};
}

#endif //AMBIENT_OCCLUSION_TYPES_H
