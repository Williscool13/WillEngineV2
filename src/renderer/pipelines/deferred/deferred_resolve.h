//
// Created by William on 2024-12-07.
//

#ifndef DEFERRED_RESOLVE_H
#define DEFERRED_RESOLVE_H

#include <vulkan/vulkan_core.h>

#include "src/core/environment.h"
#include "src/renderer/vk_descriptor_buffer.h"
#include "src/renderer/vulkan_context.h"

struct DeferredResolvePipelineCreateInfo
{
    VkDescriptorSetLayout sceneDataLayout;
    VkDescriptorSetLayout environmentMapLayout;
    VkDescriptorSetLayout emptyLayout;

    VkFormat normalFormat;
    VkFormat albedoFormat;
    VkFormat pbrFormat;
    VkFormat depthFormat;
    VkFormat velocityFormat;
    VkFormat outputFormat;
};

struct DeferredResolveDescriptorBufferInfo
{
    VkImageView normalTarget;
    VkImageView albedoTarget;
    VkImageView pbrTarget;
    VkImageView depthTarget;
    VkImageView velocityTarget;
    VkImageView outputTarget;

    VkSampler nearestSampler;
};

struct DeferredResolveDrawInfo
{
    VkExtent2D renderExtent;
    int32_t debugMode;
    DescriptorBufferUniform& sceneData;
    Environment* environment;
    int32_t environmentMapIndex;
};

class DeferredResolvePipeline
{
public:
    explicit DeferredResolvePipeline(VulkanContext& context);

    ~DeferredResolvePipeline();

    void init(const DeferredResolvePipelineCreateInfo& createInfo);

    void draw(VkCommandBuffer cmd, const DeferredResolveDrawInfo& drawInfo) const;

private:
    VulkanContext& context;
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};

    /**
     * Transient (Lifetime is managed outside of this pipeline)
     */
    VkDescriptorSetLayout sceneDataLayout{VK_NULL_HANDLE};
    /**
     * Transient (Lifetime is managed outside of this pipeline)
     */
    VkDescriptorSetLayout resolveTargetLayout{VK_NULL_HANDLE};
    /**
     * Transient (Lifetime is managed outside of this pipeline)
     */
    VkDescriptorSetLayout emptyLayout{VK_NULL_HANDLE};
    /**
     * Transient (Lifetime is managed outside of this pipeline)
     */
    VkDescriptorSetLayout environmentMapLayout{VK_NULL_HANDLE};

    DescriptorBufferSampler resolveDescriptorBuffer;

    void cleanup();

    void createDescriptorLayouts();

    void createPipelineLayout();

    void createPipeline();

    void setupDescriptorBuffer(const DeferredResolveDescriptorBufferInfo& drawInfo);
};


#endif //DEFERRED_RESOLVE_H
