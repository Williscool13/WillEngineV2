//
// Created by William on 2025-03-23.
//

#ifndef AMBIENT_OCCLUSION_TYPES_H
#define AMBIENT_OCCLUSION_TYPES_H
#include <glm/glm.hpp>

#include "engine/core/camera/camera.h"

namespace will_engine::renderer
{
static constexpr int32_t DEPTH_PREFILTER_MIP_COUNT = 5;
static constexpr int32_t GTAO_DENOISE_PASSES = 1;

static constexpr float XE_GTAO_SLICE_COUNT_LOW = 1.0f;
static constexpr float XE_GTAO_SLICE_COUNT_MEDIUM = 2.0f;
static constexpr float XE_GTAO_SLICE_COUNT_HIGH = 3.0f;
static constexpr float XE_GTAO_SLICE_COUNT_ULTRA = 9.0f;
static constexpr float XE_GTAO_STEPS_PER_SLICE_COUNT_LOW = 2.0f;
static constexpr float XE_GTAO_STEPS_PER_SLICE_COUNT_MEDIUM = 2.0f;
static constexpr float XE_GTAO_STEPS_PER_SLICE_COUNT_HIGH = 3.0f;
static constexpr float XE_GTAO_STEPS_PER_SLICE_COUNT_ULTRA = 3.0f;

#ifdef WILL_ENGINE_DEBUG
#define DEFAULT_GTAO_SLICE_COUNT XE_GTAO_SLICE_COUNT_MEDIUM
#define DEFAULT_GTAO_STEPS_PER_SLICE_COUNT XE_GTAO_STEPS_PER_SLICE_COUNT_MEDIUM
#else
#define DEFAULT_GTAO_SLICE_COUNT XE_GTAO_SLICE_COUNT_ULTRA
#define DEFAULT_GTAO_STEPS_PER_SLICE_COUNT XE_GTAO_STEPS_PER_SLICE_COUNT_ULTRA
#endif


struct GTAOPushConstants
{
    glm::vec2 cameraTanHalfFOV{};

    glm::vec2 ndcToViewMul{};
    glm::vec2 ndcToViewAdd{};

    glm::vec2 ndcToViewMul_x_PixelSize{};

    float depthLinearizeMult{0.0f};
    float depthLinearizeAdd{0.0f};

    float effectRadius = 0.5f;
    float effectFalloffRange = 0.615f;
    float denoiseBlurBeta = (GTAO_DENOISE_PASSES == 0) ? (1e4f) : (1.2f);

    float radiusMultiplier = 1.457f;
    float sampleDistributionPower = 2.0f;
    float thinOccluderCompensation = 0.0f;
    float finalValuePower = 2.2f;
    float depthMipSamplingOffset = 3.30f;
    uint32_t noiseIndex{0};
    int32_t isFinalDenoisePass{1};

    float sliceCount{DEFAULT_GTAO_SLICE_COUNT};
    float stepsPerSliceCount{DEFAULT_GTAO_STEPS_PER_SLICE_COUNT};

    int32_t debug{4};
};

struct GTAODrawInfo
{
    VkExtent2D renderExtent{DEFAULT_RENDER_EXTENT_2D};
    Camera* camera{nullptr};
    bool bEnabled{true};
    GTAOPushConstants& push;
    int32_t currentFrame{};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
};

struct GTAOSettings
{
    bool bEnabled{true};
    GTAOPushConstants pushConstants{};
};

}

#endif //AMBIENT_OCCLUSION_TYPES_H
