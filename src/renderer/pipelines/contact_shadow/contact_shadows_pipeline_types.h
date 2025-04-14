//
// Created by William on 2025-04-14.
//

#ifndef CONTACT_SHADOW_TYPES_H
#define CONTACT_SHADOW_TYPES_H

#include <volk/volk.h>

namespace will_engine::contact_shadows_pipeline
{

struct ContactShadowsPushConstants
{
    float surfaceThickness{0.005};
    float bilinearThreshold{0.02};
    float shadowContrast{4};
    int32_t ignoreEdgePixels{0};

};

}

#endif //CONTACT_SHADOW_TYPES_H
