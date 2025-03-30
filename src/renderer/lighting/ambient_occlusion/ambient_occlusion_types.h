//
// Created by William on 2025-03-23.
//

#ifndef AMBIENT_OCCLUSION_TYPES_H
#define AMBIENT_OCCLUSION_TYPES_H
#include <glm/glm.hpp>

#include "src/core/camera/camera.h"

namespace will_engine::ambient_occlusion
{
static constexpr int32_t DEPTH_PREFILTER_MIP_COUNT = 5;
static constexpr int32_t GTAO_DENOISE_PASSES = 1;

struct GTAOPushConstants
{
    glm::vec2 cameraTanHalfFOV;

    glm::vec2 ndcToViewMul;
    glm::vec2 ndcToViewAdd;

    glm::vec2 ndcToViewMul_x_PixelSize;

    float depthLinearizeMult;
    float depthLinearizeAdd;

    float effectRadius = 0.5f;
    float effectFalloffRange = 0.615f;
    float denoiseBlurBeta = (GTAO_DENOISE_PASSES == 0) ? (1e4f) : (1.2f);

    float radiusMultiplier = 1.457f;
    float sampleDistributionPower = 2.0f;
    float thinOccluderCompensation = 0.0f;
    float finalValuePower = 2.2f;
    float depthMipSamplingOffset = 3.30f;
    uint32_t noiseIndex{0};

    int32_t debug{0};
};

struct GTAODrawInfo
{
    Camera* camera{nullptr};
    GTAOPushConstants pushConstants{};
    int32_t currentFrame{};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
};
}

#endif //AMBIENT_OCCLUSION_TYPES_H
