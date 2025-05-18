//
// Created by William on 2024-11-01.
//

#ifndef HALTON_H
#define HALTON_H

#include <glm/glm.hpp>
#include <fmt/format.h>

namespace will_engine{
class HaltonSequence
{
public:
    // Pre-computed 16-point Halton sequence (base 2, 3)
    static constexpr std::array<glm::vec2, 16> HALTON_SEQUENCE = {
        glm::vec2(0.5,    0.33333334),
        {0.25,   0.66666667},
        {0.75,   0.11111111},
        {0.125,  0.44444445},
        {0.625,  0.7777778},
        {0.375,  0.22222222},
        {0.875,  0.5555556},
        {0.0625, 0.8888889},
        {0.5625, 0.037037037},
        {0.3125, 0.3703704},
        {0.8125, 0.7037037},
        {0.1875, 0.14814815},
        {0.6875, 0.4814815},
        {0.4375, 0.8148148},
        {0.9375, 0.25925925},
        {0.03125,0.5925926},
    };

    static glm::vec2 getJitterHardcoded(const uint32_t frameNumber) {
        return HALTON_SEQUENCE[frameNumber % 16];
    }

    /**
     * Get 2D jitter offset for given frame index, scaled to pixel size
     * @param frameIndex
     * @param screenDims
     * @return
     */
    static glm::vec2 getJitter(const uint32_t frameIndex, const glm::vec2& screenDims)
    {
        const float x = halton(frameIndex, 2);
        const float y = halton(frameIndex, 3);

        return {x,y};
    }

private:
    /**
     * Core Halton sequence generator
     * Returns values in range [0,1]
     * @param index
     * @param base
     * @return
     */
    static float halton(uint32_t index, const uint32_t base)
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
};
}



#endif //HALTON_H
