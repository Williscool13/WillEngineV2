//
// Created by William on 2025-01-26.
//

#ifndef SHADOW_CONSTANTS_H
#define SHADOW_CONSTANTS_H
#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>

namespace will_engine::shadows
{
static constexpr float LAMBDA = 0.5f;
static constexpr float OVERLAP = 1.005f;
static constexpr uint32_t SHADOW_CASCADE_COUNT = 4;
static constexpr float CASCADE_NEAR_PLANE = 1.0f;
static constexpr float CASCADE_FAR_PLANE = 50.0f;
extern float CASCADE_BIAS[SHADOW_CASCADE_COUNT][2];
static constexpr VkExtent2D CASCADE_EXTENT{2048, 2048};
static constexpr VkFormat CASCADE_DEPTH_FORMAT{VK_FORMAT_D32_SFLOAT};
}

#endif //SHADOW_CONSTANTS_H
