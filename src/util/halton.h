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
    /**
     * Get 2D jitter offset for given frame index, scaled to pixel size
     * @param frameIndex
     * @param screenDims
     * @return
     */
    static glm::vec2 getJitter(uint32_t frameIndex, const glm::vec2& screenDims)
    {
        float x = halton(frameIndex, 2);
        float y = halton(frameIndex, 3);

        // Convert from [0,1] to [-0.5,0.5] pixels
        x = x - 0.5f;
        y = y - 0.5f;

        constexpr float jitterScale = 1.0f;
        x *= jitterScale;
        y *= jitterScale;

        return {x / screenDims.x, y / screenDims.y};
    }

private:
    /**
     * Core Halton sequence generator
     * Returns values in range [0,1]
     * @param index
     * @param base
     * @return
     */
    static float halton(uint32_t index, uint32_t base)
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
