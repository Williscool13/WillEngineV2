//
// Created by William on 2025-01-06.
//

#ifndef SHADOW_TYPES_H
#define SHADOW_TYPES_H

#include <glm/glm.hpp>
#include <src/renderer/vk_types.h>

#include "src/core/camera/camera_types.h"
#include "src/renderer/lighting/directional_light.h"

class RenderObject;

struct ShadowMapPipelineCreateInfo
{
    VkDescriptorSetLayout modelAddressesLayout;
    VkDescriptorSetLayout shadowMapLayout;
};

struct CascadedShadowMapDrawInfo
{
    std::vector<RenderObject*> renderObjects{};
    CameraProperties cameraProperties;
    DirectionalLight directionalLight;
    int32_t currentFrameOverlap;
};


struct ShadowMapPushConstants
{
    glm::mat4 lightMatrix{};
};

struct CascadeShadowMapData
{
    int32_t cascadeLevel{-1};
    AllocatedImage drawShadowMap{VK_NULL_HANDLE}; // debugging for now
    AllocatedImage depthShadowMap{VK_NULL_HANDLE};
};

#endif //SHADOW_TYPES_H
