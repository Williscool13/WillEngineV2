//
// Created by William on 2025-01-18.
//

#ifndef SHADOW_CONSTANTS_H
#define SHADOW_CONSTANTS_H

#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>

namespace shadow_constants
{
    static constexpr float LAMBDA = 0.5f;
    static constexpr float OVERLAP = 1.005f;
    static constexpr uint32_t SHADOW_CASCADE_COUNT = 4;
    static constexpr float CASCADE_NEAR = 0.1f;
    static constexpr float CASCADE_FAR = 100.0f;
    static constexpr glm::vec2 CASCADE_BIAS[SHADOW_CASCADE_COUNT] = {
        {0.0f, 0.0f},
        {0.0f, 0.0f},
        {0.0f, 0.0f},
        {0.0f, 0.0f},
        // {5.0f, 7.5f},
        // {9.0f, 7.0f},
        // {8.0f, 6.5f},
        // {5.0f, 6.0f},
        //{8.0f, 6.0f},
        //{6.0f, 4.5f},
        //{4.0f, 3.0f},
    };

    //static constexpr VkExtent2D cascadeExtents[SHADOW_CASCADE_COUNT] = {};
    static constexpr VkExtent2D CASCADE_EXTENT{4096, 4096};
}

#endif //SHADOW_CONSTANTS_H
