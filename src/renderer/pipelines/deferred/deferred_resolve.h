//
// Created by William on 2024-12-07.
//

#ifndef DEFERRED_RESOLVE_H
#define DEFERRED_RESOLVE_H

#include <vulkan/vulkan_core.h>

#include "src/renderer/vk_descriptor_buffer.h"
#include "src/renderer/vulkan_context.h"
#include "src/renderer/environment/environment.h"

struct DeferredResolvePipelineCreateInfo
{
    VkDescriptorSetLayout sceneDataLayout;
    VkDescriptorSetLayout environmentMapLayout;
    VkDescriptorSetLayout emptyLayout;
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
    VkExtent2D renderExtent{};
    int32_t debugMode{};
    const DescriptorBufferUniform& sceneData;
    Environment* environment{nullptr};
    int32_t environmentMapIndex{};

    explicit DeferredResolveDrawInfo(const DescriptorBufferUniform& sceneData) : sceneData(sceneData) {}
};

class DeferredResolvePipeline
{
public:
    explicit DeferredResolvePipeline(VulkanContext& context);

    ~DeferredResolvePipeline();

    void init(const DeferredResolvePipelineCreateInfo& createInfo);

    void draw(VkCommandBuffer cmd, const DeferredResolveDrawInfo& drawInfo) const;

    void setupDescriptorBuffer(const DeferredResolveDescriptorBufferInfo& drawInfo);

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
};


#endif //DEFERRED_RESOLVE_H
