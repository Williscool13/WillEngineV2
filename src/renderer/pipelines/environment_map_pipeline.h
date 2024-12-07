//
// Created by William on 2024-12-07.
//

#ifndef ENVIRONMENT_MAP_PIPELINE_H
#define ENVIRONMENT_MAP_PIPELINE_H


#include <vulkan/vulkan_core.h>
#include "src/renderer/vk_descriptor_buffer.h"
#include "src/renderer/vulkan_context.h"
#include "src/core/environment.h"

struct EnvironmentPipelineCreateInfo
{
    VkDescriptorSetLayout sceneDataLayout;
    VkDescriptorSetLayout environmentMapLayout;
    VkFormat colorFormat;
    VkFormat depthFormat;
};

struct EnvironmentDrawInfo
{
    VkExtent2D renderExtent;
    VkImageView colorAttachment;
    VkImageView depthAttachment;
    DescriptorBufferUniform& sceneData;
    Environment* environment;
    int32_t environmentMapIndex;
};

struct EnvironmentPipelineRenderInfo
{
    VkFormat colorFormat;
    VkFormat depthFormat;
};

class EnvironmentPipeline
{
public:
    explicit EnvironmentPipeline(VulkanContext& context);

    ~EnvironmentPipeline();

    void init(const EnvironmentPipelineCreateInfo& createInfo, const EnvironmentPipelineRenderInfo& renderInfo);

    void draw(VkCommandBuffer cmd, const EnvironmentDrawInfo& drawInfo) const;

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
    VkDescriptorSetLayout environmentMapLayout{VK_NULL_HANDLE};
    EnvironmentPipelineRenderInfo renderFormats{};

    void cleanup();

    void createPipelineLayout();

    void createPipeline();
};

#endif //ENVIRONMENT_MAP_PIPELINE_H
