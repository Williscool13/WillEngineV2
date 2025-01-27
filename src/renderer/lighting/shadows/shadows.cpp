//
// Created by William on 2025-01-27.
//

#include "shadows.h"

float will_engine::shadows::CASCADE_BIAS[SHADOW_CASCADE_COUNT][2] = {
    {10.0f, 9.0f},
    {10.0f, 5.0f},
    {10.0f, 3.0f},
    {10.0f, 2.0f},
};
