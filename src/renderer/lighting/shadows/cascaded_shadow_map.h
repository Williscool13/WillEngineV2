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
    static constexpr float LAMBDA = 0.5f;
    static constexpr float OVERLAP = 1.005f;
    static constexpr uint32_t SHADOW_CASCADE_COUNT = 4;
    static constexpr uint32_t SHADOW_CASCADE_SPLIT_COUNT = SHADOW_CASCADE_COUNT + 1;

    CascadedShadowMap() = delete;

    CascadedShadowMap(const VulkanContext& context, ResourceManager& resourceManager);

    ~CascadedShadowMap();

    void init(const ShadowMapPipelineCreateInfo& shadowMapPipelineCreateInfo);

    void updateCascadeData();

    void draw(VkCommandBuffer cmd, const CascadedShadowMapDrawInfo& drawInfo);

public: // Debug
    AllocatedImage getShadowMap(const int32_t cascadeLevel) const
    {
        if (cascadeLevel >= SHADOW_CASCADE_SPLIT_COUNT || cascadeLevel < 0) {
            return {};
        }
        return shadowMaps[cascadeLevel].depthShadowMap;
    }

    glm::mat4 getCascadeViewProjection(const glm::vec3 directionalLightDirection, const CameraProperties& cameraProperties, const int32_t cascadeLevel)
    {
        if (cascadeLevel >= SHADOW_CASCADE_SPLIT_COUNT || cascadeLevel < 0) {
            return {1.0f};
        }

        glm::mat4 lightMatrices[SHADOW_CASCADE_SPLIT_COUNT];
        getLightSpaceMatrices(directionalLightDirection, cameraProperties, lightMatrices);

        return lightMatrices[cascadeLevel];
    }

    const CascadeSplit& getCascadeSplit(const int32_t index) const
    {
        if (index >= SHADOW_CASCADE_SPLIT_COUNT || index < 0) {
            return splits[0];
        }
        return splits[index];
    }

    void getLightSpaceMatrices(glm::vec3 directionalLightDirection, const CameraProperties& cameraProperties, glm::mat4 matrices[SHADOW_CASCADE_SPLIT_COUNT]) const;

    static glm::mat4 getLightSpaceMatrix(glm::vec3 lightDirection, const CameraProperties& cameraProperties, float cascadeNear, float cascadeFar);

    /**
     * Assumes reversed depth buffer
     * @param nearPlane Exepects a large number (e.g. 10000)
     * @param farPlane Expects a small number (e.g. 0.1)
     */
    void generateSplits(float nearPlane, float farPlane);

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

    CascadeShadowMapData shadowMaps[SHADOW_CASCADE_SPLIT_COUNT]{
        {0, {VK_NULL_HANDLE}},
        {1, {VK_NULL_HANDLE}},
        {2, {VK_NULL_HANDLE}},
        {3, {VK_NULL_HANDLE}},
        {4, {VK_NULL_HANDLE}},
    };

    CascadeSplit splits[SHADOW_CASCADE_SPLIT_COUNT]{};

    // contains the depth maps used by deferred resolve
    DescriptorBufferSampler cascadedShadowMapDescriptorBufferSampler;
    // contains the cascaded shadow map properties used by deferred resolve
    AllocatedBuffer cascadedShadowMapData{VK_NULL_HANDLE};
    DescriptorBufferUniform cascadedShadowMapDescriptorBufferUniform;
};


#endif //SHADOW_MAP_H
