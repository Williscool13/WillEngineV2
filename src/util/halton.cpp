//
// Created by William on 2024-11-01.
//

#include "halton.h"

glm::vec2 HaltonSequence::getJitter(const uint32_t frameIndex, const glm::vec2& screenDims)
{
    float x = halton(frameIndex, 2);
    float y = halton(frameIndex, 3);

    // Scale to screen dims
    return {(x - 0.5f) / screenDims.x, (y - 0.5f) / screenDims.y};
}


float HaltonSequence::halton(uint32_t index, const uint32_t base)
{
    float f = 1.0f;
    float r = 0.0f;

    index = (index + 1); // Start from index 1 to avoid (0,0) jitter
    while (index > 0) {
        f /= static_cast<float>(base);
        r += f * static_cast<float>(index % base);
        index /= base;
    }

    return r;
}
