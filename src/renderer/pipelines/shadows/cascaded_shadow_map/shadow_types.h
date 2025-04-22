//
// Created by William on 2025-01-26.
//

#ifndef SHADOW_TYPES_H
#define SHADOW_TYPES_H

#include <glm/glm.hpp>

#include "src/renderer/vk_types.h"
#include "src/renderer/lighting/directional_light.h"

namespace will_engine::cascaded_shadows
{
static inline constexpr uint32_t SHADOW_CASCADE_COUNT{4};
static inline VkFormat CASCADE_DEPTH_FORMAT{VK_FORMAT_D32_SFLOAT};

struct CascadeBias
{
    float linearBias{0.0f};
    float slopedBias{0.0f};
};

struct CascadedShadowMapProperties
{
    float splitLambda = 0.8f;
    float splitOverlap = 1.05f;

    float cascadeNearPlane{0.1f};
    float cascadeFarPlane{100.0f};

    std::array<cascaded_shadows::CascadeBias, SHADOW_CASCADE_COUNT> cascadeBias{
        CascadeBias(500.0f, 0.7f),
        {500.0f, 0.6f},
        {600.0f, 1.0f},
        {300.0f, 1.0f},
    };

    /**
     * Don't modify from defaults, csm not set up to react to that
     */
    std::array<VkExtent3D, SHADOW_CASCADE_COUNT> cascadeExtents{
        VkExtent3D(2048, 2048, 1),
        {2048, 2048, 1},
        {2048, 2048, 1},
        {2048, 2048, 1}
    };
};

struct CascadeSplit
{
    float nearPlane;
    float farPlane;
    float padding[2];
};

struct CascadeShadowMapData
{
    int32_t cascadeLevel{-1};
    CascadeSplit split{};
    AllocatedImage depthShadowMap{VK_NULL_HANDLE};
    glm::mat4 lightViewProj{};
};

struct CascadedShadowMapGenerationPushConstants
{
    int32_t cascadeIndex{};
    int32_t tessLevel{};
};

struct CascadeShadowData
{
    CascadeSplit cascadeSplits[4];
    glm::mat4 lightViewProj[4];
    DirectionalLightData directionalLightData;
    float nearShadowPlane;
    float farShadowPlane;
    glm::vec2 pad;
};
}
#endif //SHADOW_TYPES_H
