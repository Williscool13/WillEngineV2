//
// Created by William on 2025-01-01.
//

#ifndef SHADOW_MAP_H
#define SHADOW_MAP_H

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

#include "cascaded_shadow_map_descriptor_layouts.h"
#include "shadow_constants.h"
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

    void updateCascadeData(VkCommandBuffer cmd, const DirectionalLight& mainLight, const CameraProperties& cameraProperties);

    void draw(VkCommandBuffer cmd, const CascadedShadowMapDrawInfo& drawInfo) const;

    const DescriptorBufferUniform& getCascadedShadowMapUniformBuffer() const { return cascadedShadowMapDescriptorBufferUniform; }
    const DescriptorBufferSampler& getCascadedShadowMapSamplerBuffer() const { return cascadedShadowMapDescriptorBufferSampler; }

public: // Debug
    AllocatedImage getShadowMap(const int32_t cascadeLevel) const
    {
        if (cascadeLevel >= shadow_constants::SHADOW_CASCADE_COUNT || cascadeLevel < 0) {
            return {};
        }
        return shadowMaps[cascadeLevel].depthShadowMap;
    }

    glm::mat4 getCascadeViewProjection(const int32_t cascadeLevel) const
    {
        if (cascadeLevel >= shadow_constants::SHADOW_CASCADE_COUNT || cascadeLevel < 0) {
            return {1.0f};
        }

        return shadowMaps[cascadeLevel].lightViewProj;
    }

    const CascadeSplit& getCascadeSplit(const int32_t index) const
    {
        if (index >= shadow_constants::SHADOW_CASCADE_COUNT || index < 0) {
            return shadowMaps[0].split;
        }
        return shadowMaps[index].split;
    }

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

    VkFormat depthFormat{VK_FORMAT_D32_SFLOAT};
    //VkExtent2D extent{2048, 2048};
    VkExtent2D extent{4096, 4096};

    VkSampler sampler{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};

    CascadeShadowMapData shadowMaps[shadow_constants::SHADOW_CASCADE_COUNT]{
        {0, {}, {VK_NULL_HANDLE}, {}},
        {1, {}, {VK_NULL_HANDLE}, {}},
        {2, {}, {VK_NULL_HANDLE}, {}},
        {3, {}, {VK_NULL_HANDLE}, {}},
    };

    CascadeSplit splits[shadow_constants::SHADOW_CASCADE_COUNT]{};

    // contains the depth maps used by deferred resolve
    DescriptorBufferSampler cascadedShadowMapDescriptorBufferSampler;
    // contains the cascaded shadow map properties used by deferred resolve
    AllocatedBuffer cascadedShadowMapData{VK_NULL_HANDLE};
    DescriptorBufferUniform cascadedShadowMapDescriptorBufferUniform;
};


#endif //SHADOW_MAP_H
