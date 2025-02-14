//
// Created by William on 2025-01-27.
//

#include "shadows.h"

float will_engine::shadows::CASCADE_BIAS[SHADOW_CASCADE_COUNT][2] = {
    {750.0f, 6.0f},
    {400.0f, 4.0f},
    {100.0f, 3.0f},
    {100.0f, 2.0f},
};
