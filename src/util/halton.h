//
// Created by William on 2024-11-01.
//

#ifndef HALTON_H
#define HALTON_H

#include <glm/glm.hpp>

class HaltonSequence {
public:
    /**
     * Get 2D jitter offset for given frame index, scaled to pixel size
     * @param frameIndex
     * @param screenDims
     * @return
     */
    static glm::vec2 getJitter(uint32_t frameIndex, const glm::vec2& screenDims);


private:
    /**
     * Core Halton sequence generator
     * Returns values in range [0,1]
     * @param index
     * @param base
     * @return
     */
    static float halton(uint32_t index, uint32_t base);

};

class halton {

};



#endif //HALTON_H
