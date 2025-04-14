//
// Created by William on 2025-04-14.
//

#ifndef CONTACT_SHADOW_TYPES_H
#define CONTACT_SHADOW_TYPES_H

#include <glm/glm.hpp>
#include <volk/volk.h>

namespace will_engine::contact_shadows_pipeline
{
enum class ContactShadowsDebugMode : int32_t
{
    NONE = 0,
};

struct ContactShadowsPushConstants
{
    float surfaceThickness{0.005};
    float bilinearThreshold{0.02};
    float shadowContrast{4};

    int32_t bIgnoreEdgePixels{0};
    int32_t bUsePrecisionOffset{0};
    int32_t bBilinearSamplingOffsetMode{0};

    glm::vec2 depthBounds{0, 1};
    int32_t bUseEarlyOut{0};

    ContactShadowsDebugMode debugMode{ContactShadowsDebugMode::NONE};
};

struct ContactShadowsDrawInfo
{

};
}

#endif //CONTACT_SHADOW_TYPES_H
