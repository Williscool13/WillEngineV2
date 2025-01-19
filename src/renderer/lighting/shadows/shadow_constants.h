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
    static constexpr float CASCADE_FAR = 50.0f;
    static constexpr glm::vec2 CASCADE_BIAS[SHADOW_CASCADE_COUNT] = {
        {1.75f, 3.75f},
        {1.50f, 3.25f},
        {1.25f, 2.75f},
        {0.5f, 2.75f},
        // {2.0f, 4.0f},
        // {2.25f, 4.25f},
        // {3.25f, 5.25f},
    };

    //static constexpr VkExtent2D cascadeExtents[SHADOW_CASCADE_COUNT] = {};
    static constexpr VkExtent2D CASCADE_EXTENT{2048, 2048};
    //static constexpr VkExtent2D CASCADE_EXTENT{4096, 4096};
}

#endif //SHADOW_CONSTANTS_H
