//
// Created by William on 2025-01-01.
//

#ifndef SHADOW_MAP_H
#define SHADOW_MAP_H

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

#include "cascaded_shadow_map_descriptor_layouts.h"
#include "shadow_types.h"
#include "src/renderer/vk_descriptor_buffer.h"
#include "src/renderer/vk_types.h"

class DirectionalLight;
struct ShadowMapPipelineCreateInfo;
struct CameraProperties;

class CascadedShadowMap
{
public:
    CascadedShadowMap() = delete;

    CascadedShadowMap(const VulkanContext& context, ResourceManager& resourceManager);

    ~CascadedShadowMap();

    void init(const ShadowMapPipelineCreateInfo& shadowMapPipelineCreateInfo);

    void updateCascadeData();

    void draw(VkCommandBuffer cmd, const CascadedShadowMapDrawInfo& drawInfo);

public: // Debug
    AllocatedImage getShadowMap(const int32_t cascadeLevel) const
    {
        if (cascadeLevel >= SHADOW_MAP_COUNT || cascadeLevel < 0) {
            return {};
        }
        return shadowMaps[cascadeLevel].depthShadowMap;
    }

    static glm::mat4 getCascadeViewProjection(const glm::vec3 directionalLightDirection, const CameraProperties& cameraProperties, const int32_t cascadeLevel)
    {
        if (cascadeLevel >= SHADOW_MAP_COUNT || cascadeLevel < 0) {
            return {1.0f};
        }

        glm::mat4 lightMatrices[SHADOW_MAP_COUNT];
        getLightSpaceMatrices(directionalLightDirection, cameraProperties, lightMatrices);

        return lightMatrices[cascadeLevel];
    }

    static float getCascadeLevel(const int32_t index)
    {
        if (index >= SHADOW_CASCADE_COUNT || index < 0) {
            return normalizedCascadeLevels[0];
        }
        return normalizedCascadeLevels[index];
    }

    //private: // Pipelines
    static void getLightSpaceMatrices(glm::vec3 directionalLightDirection, const CameraProperties& cameraProperties, glm::mat4 matrices[SHADOW_MAP_COUNT]);

    static glm::mat4 getLightSpaceMatrix(glm::vec3 lightDirection, const CameraProperties& cameraProperties, float cascadeNear, float cascadeFar);


    VkPipelineLayout shadowMapGenerationPipelineLayout{VK_NULL_HANDLE};
    VkPipeline shadowMapGenerationPipeline{VK_NULL_HANDLE};

private:
    const VulkanContext& context;
    ResourceManager& resourceManager;

    VkFormat drawFormat{VK_FORMAT_R16G16B16A16_SFLOAT};
    VkFormat depthFormat{VK_FORMAT_D32_SFLOAT};
    VkExtent2D extent{2048, 2048}; // Default shadow map resolution


    VkSampler sampler{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};

    // 0 = near -> 0.02
    // 1 = 0.02 -> 0.08
    // 2 = 0.08 -> 0.25
    // 3 = 0.25 -> 0.6
    // 4 = 0.6 -> far
    CascadeShadowMapData shadowMaps[SHADOW_MAP_COUNT]{
        {0, {VK_NULL_HANDLE}},
        {1, {VK_NULL_HANDLE}},
        {2, {VK_NULL_HANDLE}},
        {3, {VK_NULL_HANDLE}},
        {4, {VK_NULL_HANDLE}},
    };

    static constexpr float normalizedCascadeLevels[SHADOW_CASCADE_COUNT]{0.02f, 0.08f, 0.25f, 0.6f};


    // contains the depth maps used by deferred resolve
    DescriptorBufferSampler cascadedShadowMapDescriptorBufferSampler;
    // contains the cascaded shadow map properties used by deferred resolve
    AllocatedBuffer cascadedShadowMapData{VK_NULL_HANDLE};
    DescriptorBufferUniform cascadedShadowMapDescriptorBufferUniform;
};


#endif //SHADOW_MAP_H
