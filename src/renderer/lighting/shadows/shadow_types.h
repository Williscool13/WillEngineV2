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
    VkDescriptorSetLayout cascadedShadowMapSamplerLayout;
    VkDescriptorSetLayout cascadedShadowMapUniformLayout;
    float nearPlane;
    float farPlane;
};

struct CascadedShadowMapDrawInfo
{
    std::vector<RenderObject*> renderObjects{};
    CameraProperties cameraProperties;
    DirectionalLight directionalLight;
};


struct CascadedShadowMapGenerationPushConstants
{
    int32_t cascadeIndex{};
};

struct CascadeSplit
{
    float nearPlane;
    float farPlane;
    float padding[2];
};
struct CascadeShadowData
{
    CascadeSplit cascadeSplits[4];
    glm::mat4 lightViewProj[4];
    DirectionalLightData directionalLightData;
};

struct CascadeShadowMapData
{
    int32_t cascadeLevel{-1};
    CascadeSplit split{};
    AllocatedImage depthShadowMap{VK_NULL_HANDLE};
    glm::mat4 lightViewProj{};
};

#endif //SHADOW_TYPES_H
