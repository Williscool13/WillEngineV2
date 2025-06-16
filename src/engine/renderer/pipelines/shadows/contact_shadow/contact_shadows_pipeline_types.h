//
// Created by William on 2025-04-14.
//

#ifndef CONTACT_SHADOW_TYPES_H
#define CONTACT_SHADOW_TYPES_H

#include <glm/glm.hpp>
#include <volk/volk.h>

#include "engine/core/camera/camera.h"
#include "engine/renderer/lighting/directional_light.h"

namespace will_engine::renderer
{

static constexpr int32_t CONTACT_SHADOW_WAVE_SIZE = 64;

struct DispatchData
{
    int32_t WaveCount[3];					// Compute Shader Dispatch(X,Y,Z) wave counts X/Y/Z
    int32_t WaveOffset_Shader[2];			// This value is passed in to shader. It will be different for each dispatch
};

struct DispatchList
{
    float LightCoordinate_Shader[4];	    // This value is passed in to shader, this will be the same value for all dispatches for this light

    DispatchData Dispatch[8];			    // List of dispatches (max count is 8)
    int32_t DispatchCount;					// Number of compute dispatches written to the list
};


struct ContactShadowsPushConstants
{
    float surfaceThickness{0.005};
    float bilinearThreshold{0.05};
    float shadowContrast{2};

    int32_t bIgnoreEdgePixels{1};
    int32_t bUsePrecisionOffset{0};
    int32_t bBilinearSamplingOffsetMode{0};

    glm::vec2 depthBounds{0, 1};
    /**
     * Doesn't do anything.
     */
    int32_t bUseEarlyOut{1};

    int32_t debugMode{0};

    glm::ivec2 waveOffset{0};
    glm::vec4 lightCoordinate{0.0f};
};

struct ContactShadowsDrawInfo
{
    Camera* camera;
    DirectionalLight light;
    bool bIsEnabled{true};
    ContactShadowsPushConstants push;
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
};

struct ContactShadowSettings
{
    bool bEnabled{true};
    ContactShadowsPushConstants pushConstants{};
};

}

#endif //CONTACT_SHADOW_TYPES_H
