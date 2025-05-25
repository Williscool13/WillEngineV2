//
// Created by William on 2025-01-26.
//

#ifndef SHADOW_TYPES_H
#define SHADOW_TYPES_H

#include <unordered_set>
#include <glm/glm.hpp>
#include <volk/volk.h>


#include "engine/renderer/lighting/directional_light.h"
#include "engine/renderer/resources/allocated_image.h"

namespace will_engine
{
class ITerrain;
}

namespace will_engine::renderer
{
class RenderObject;

static inline constexpr uint32_t SHADOW_CASCADE_COUNT{4};
static inline constexpr VkFormat CASCADE_DEPTH_FORMAT{VK_FORMAT_D32_SFLOAT};

struct CascadeBias
{
    float constant{0.0f};
    float slope{0.0f};
};

struct CascadedShadowMapSettings
{
    bool bEnabled{true};
    int32_t pcfLevel{1};

    float splitLambda = 0.8f;
    float splitOverlap = 1.05f;

    float cascadeNearPlane{0.1f};
    float cascadeFarPlane{100.0f};

    std::array<CascadeBias, SHADOW_CASCADE_COUNT> cascadeBias{
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

    bool useManualSplit{true};

    std::array<float, SHADOW_CASCADE_COUNT + 1> manualCascadeSplits{
        0.1,
        4,
        25,
        100,
        400,
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
    AllocatedImage depthShadowMap{};
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

struct CascadedShadowMapDrawInfo
{
    bool bEnabled{true};
    int32_t currentFrameOverlap{};
    std::vector<RenderObject*>& renderObjects;
    std::unordered_set<ITerrain*>& terrains;
};
}
#endif //SHADOW_TYPES_H
