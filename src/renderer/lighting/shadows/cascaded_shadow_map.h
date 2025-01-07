//
// Created by William on 2025-01-01.
//

#ifndef SHADOW_MAP_H
#define SHADOW_MAP_H

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

#include "shadow_map_descriptor_layouts.h"
#include "shadow_types.h"
#include "glm/detail/_noise.hpp"
#include "glm/detail/_noise.hpp"
#include "glm/gtx/associated_min_max.hpp"
#include "glm/gtx/associated_min_max.hpp"
#include "src/renderer/vk_descriptor_buffer.h"
#include "src/renderer/vk_types.h"

class DirectionalLight;
struct ShadowMapPipelineCreateInfo;
struct CameraProperties;

constexpr uint32_t SHADOW_CASCADE_COUNT = 4;
constexpr uint32_t SHADOW_MAP_COUNT = SHADOW_CASCADE_COUNT + 1;

class CascadedShadowMap
{
public:
    CascadedShadowMap() = delete;

    CascadedShadowMap(const VulkanContext& context, ResourceManager& resourceManager);

    ~CascadedShadowMap();

    void init(const ShadowMapPipelineCreateInfo& shadowMapPipelineCreateInfo);

    void draw(VkCommandBuffer cmd, const CascadedShadowMapDrawInfo& drawInfo);

public: // Debug
    AllocatedImage getShadowMap(const int32_t cascadeLevel) const
    {
        if (cascadeLevel >= SHADOW_CASCADE_COUNT || cascadeLevel < 0) {
            return {};
        }
        return shadowMaps[cascadeLevel].depthShadowMap;
    }

    static glm::mat4 getCascadeViewProjection(const glm::vec3 directionalLightDirection, const CameraProperties& cameraProperties, const int32_t cascadeLevel)
    {
        if (cascadeLevel >= SHADOW_CASCADE_COUNT || cascadeLevel < 0) {
            return {1.0f};
        }

        glm::mat4 lightMatrices[SHADOW_MAP_COUNT];
        getLightSpaceMatrices(directionalLightDirection, cameraProperties, lightMatrices);

        return lightMatrices[cascadeLevel];
    }

private: // Pipelines
    static void getLightSpaceMatrices(glm::vec3 directionalLightDirection, const CameraProperties& cameraProperties, glm::mat4 matrices[SHADOW_MAP_COUNT]);

    static void getFrustumCornersWorldSpace(const ::CameraProperties& cameraProperties, float nearPlane, float farPlane, glm::vec4 corners[8]);

    static glm::mat4 getLightSpaceMatrix(glm::vec3 directionalLightDirection, const CameraProperties& cameraProperties, float cascadeNear, float cascadeFar);


    VkPipelineLayout shadowMapGenerationPipelineLayout{VK_NULL_HANDLE};
    VkPipeline shadowMapGenerationPipeline{VK_NULL_HANDLE};

private:
    const VulkanContext& context;
    ResourceManager& resourceManager;

    VkFormat depthFormat{VK_FORMAT_D32_SFLOAT};
    VkExtent2D extent{2048, 2048}; // Default shadow map resolution


    VkSampler sampler{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};

    // 0 = near -> 0.98
    // 1 = 0.98 -> 0.92
    // 2 = 0.92 -> 0.75
    // 3 = 0.75 -> 0.40
    // 4 = 0.40 -> far
    CascadeShadowMapData shadowMaps[SHADOW_MAP_COUNT] {
        {0, {VK_NULL_HANDLE}},
        {1, {VK_NULL_HANDLE}},
        {2, {VK_NULL_HANDLE}},
        {3, {VK_NULL_HANDLE}},
        {4, {VK_NULL_HANDLE}},
    };

    static constexpr float normalizedCascadeLevels[SHADOW_CASCADE_COUNT]{0.98f, 0.92f, 0.75f, 0.4f};

    // used to generate
    DescriptorBufferSampler equiImageDescriptorBuffer;
    // used by deferred resolve
    DescriptorBufferSampler shadowMapDescriptorBuffer;
};


#endif //SHADOW_MAP_H
