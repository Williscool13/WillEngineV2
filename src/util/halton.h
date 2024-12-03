//
// Created by William on 2024-11-01.
//

#ifndef HALTON_H
#define HALTON_H

#include <glm/glm.hpp>
#include <fmt/format.h>

class HaltonSequence
{
public:
    // Pre-computed 8-point Halton sequence (base 2, 3)
    static constexpr glm::vec2 HALTON_SEQUENCE[8] = {
        glm::vec2(0.125f, 0.111111f),
        glm::vec2(0.625f, 0.444444f),
        glm::vec2(0.375f, 0.777778f),
        glm::vec2(0.875f, 0.222222f),
        glm::vec2(0.250f, 0.555556f),
        glm::vec2(0.750f, 0.888889f),
        glm::vec2(0.500f, 0.333333f),
        glm::vec2(1.000f, 0.666667f)
    };

    static glm::vec2 getJitterHardcoded(const uint32_t frameNumber, const glm::vec2& screenDims) {
        // Get the sequence point for this frame (wrapping around every 8 frames)
        const glm::vec2 halton = HALTON_SEQUENCE[frameNumber % 8];

        return (halton * 2.0f - 1.f) / screenDims;
        //return (halton * 0.5f) / screenDims;
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

        return (glm::vec2(x, y) * 2.0f - 1.0f) / screenDims;
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


#endif //HALTON_H
