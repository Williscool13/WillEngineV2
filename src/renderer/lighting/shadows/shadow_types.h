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

static constexpr uint32_t SHADOW_CASCADE_COUNT = 4;
static constexpr uint32_t SHADOW_MAP_COUNT = SHADOW_CASCADE_COUNT + 1;


struct ShadowMapPipelineCreateInfo
{
    VkDescriptorSetLayout modelAddressesLayout;
    VkDescriptorSetLayout cascadedShadowMapSamplerLayout;
    VkDescriptorSetLayout cascadedShadowMapUniformLayout;
};

struct CascadedShadowMapDrawInfo
{
    std::vector<RenderObject*> renderObjects{};
    CameraProperties cameraProperties;
    DirectionalLight directionalLight;
};


struct CascadedShadowMapGenerationPushConstants
{
    glm::mat4 lightMatrix{};
};

struct CascadeShadowMapData
{
    int32_t cascadeLevel{-1};
    AllocatedImage depthShadowMap{VK_NULL_HANDLE};
};

struct DirectionalLightData
{
    glm::vec3 direction;
    float intensity;
    glm::vec3 color;
    float pad;
};

struct CascadeShadowData
{
    float cascadeSplits[4];
    glm::mat4 lightViewProj[4];
    DirectionalLightData directionalLightData;
};

#endif //SHADOW_TYPES_H
