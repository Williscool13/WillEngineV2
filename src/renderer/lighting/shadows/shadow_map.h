//
// Created by William on 2025-01-01.
//

#ifndef SHADOW_MAP_H
#define SHADOW_MAP_H

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

#include "src/renderer/vk_descriptor_buffer.h"
#include "src/renderer/vk_types.h"
#include "src/renderer/vulkan_context.h"

class ShadowMapDescriptorLayouts;
constexpr uint32_t MAX_SHADOW_CASCADES = 4;

struct CascadeData {
    glm::mat4 viewProjMatrix;
    glm::vec4 splitDepth;  // xyz = cascade splits, w = cascade count
};

struct ShadowMapData {
    CascadeData cascades[MAX_SHADOW_CASCADES];
    glm::vec4 lightDir;
    glm::vec4 shadowMapSize; // xy = dimensions, z = bias, w = pcf samples
};

class ShadowMap {
public:
    ShadowMap() = delete;
    ShadowMap(const VulkanContext& context, ResourceManager& resourceManager, ShadowMapDescriptorLayouts& shadowMapDescriptorLayouts);
    ~ShadowMap();

    static void getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view, glm::vec4 corners[8]);

private: // Pipelines

    VkPipelineLayout shadowMapGenerationPipelineLayout{VK_NULL_HANDLE};
    VkPipeline shadowMapGenerationPipeline{VK_NULL_HANDLE};

private:
    const VulkanContext& context;
    ResourceManager& resourceManager;
    ShadowMapDescriptorLayouts& descriptorLayouts;

    VkFormat format{VK_FORMAT_D32_SFLOAT};
    VkExtent2D extent{2048, 2048};  // Default shadow map resolution


    VkSampler sampler{VK_NULL_HANDLE};

    AllocatedImage shadowMaps[MAX_SHADOW_CASCADES];

    // used to generate
    DescriptorBufferSampler equiImageDescriptorBuffer;
    // used by deferred resolve
    DescriptorBufferSampler shadowMapDescriptorBuffer;
};



#endif //SHADOW_MAP_H
