//
// Created by William on 2025-01-01.
//

#ifndef SHADOW_MAP_H
#define SHADOW_MAP_H

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

#include "shadow_map_descriptor_layouts.h"
#include "src/renderer/vk_descriptor_buffer.h"
#include "src/renderer/vk_types.h"

struct ShadowMapPipelineCreateInfo;
struct CameraProperties;

constexpr uint32_t MAX_SHADOW_CASCADES = 4;

class CascadedShadowMap
{
public:
    CascadedShadowMap() = delete;

    CascadedShadowMap(const VulkanContext& context, ResourceManager& resourceManager);

    ~CascadedShadowMap();

    void init(const ShadowMapPipelineCreateInfo& shadowMapPipelineCreateInfo);

    void draw(VkCommandBuffer cmd);

private: // Pipelines
    static void getLightSpaceMatrices(glm::vec3 directionalLightDirection, const CameraProperties& cameraProperties, glm::mat4 matrices[MAX_SHADOW_CASCADES + 1]);

    static void getFrustumCornersWorldSpace(const glm::mat4& viewProj, glm::vec4 corners[8]);

    static void getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view, glm::vec4 corners[8])
    {
        getFrustumCornersWorldSpace(proj * view, corners);
    }

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

    AllocatedImage shadowMaps[MAX_SHADOW_CASCADES]{};

    static constexpr float normalizedCascadeLevels[MAX_SHADOW_CASCADES]{0.98f, 0.92f, 0.75f, 0.4f};

    // used to generate
    DescriptorBufferSampler equiImageDescriptorBuffer;
    // used by deferred resolve
    DescriptorBufferSampler shadowMapDescriptorBuffer;
};


#endif //SHADOW_MAP_H
