//
// Created by William on 2025-01-27.
//

#include "shadows.h"

float will_engine::shadows::CASCADE_BIAS[SHADOW_CASCADE_COUNT][2] = {
    {400.0f, 4.0f},
    {200.0f, 3.0f},
    {150.0f, 2.5f},
    {100.0f, 2.0f},
};
