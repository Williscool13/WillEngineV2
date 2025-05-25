//
// Created by William on 2025-04-14.
//

#ifndef TEMPORAL_ANTIALIASING_PIPELINE_TYPES_H
#define TEMPORAL_ANTIALIASING_PIPELINE_TYPES_H

#include <volk/volk.h>

namespace will_engine::temporal_antialiasing_pipeline
{
struct TemporalAntialiasingSettings
{
    bool bEnabled{true};
    float blendValue{0.1f};
};
} // temporal_antialiasing_pipeline

#endif //TEMPORAL_ANTIALIASING_PIPELINE_TYPES_H
