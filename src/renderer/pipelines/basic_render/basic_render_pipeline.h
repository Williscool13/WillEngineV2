//
// Created by William on 2025-01-22.
//

#ifndef BASIC_RENDER_PIPELINE_H
#define BASIC_RENDER_PIPELINE_H
#include "src/renderer/vulkan_context.h"
#include "src/renderer/vulkan/descriptor_buffer/descriptor_buffer_sampler.h"
#include "src/renderer/vulkan/descriptor_buffer/descriptor_buffer_uniform.h"


namespace basic_render
{

struct RenderPipelineInfo
{
    VkFormat colorFormat;
    VkFormat depthFormat;
    VkDescriptorSetLayout sceneDataLayout;
};

struct RenderDescriptorInfo
{
    VkSampler sampler{VK_NULL_HANDLE};
    VkImageView texture{VK_NULL_HANDLE};
};

struct RenderDrawInfo
{
    VkExtent2D renderExtent{};
    VkImageView drawImage{VK_NULL_HANDLE};
    VkImageView depthImage{VK_NULL_HANDLE};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
    int32_t currentFrame{};
};

struct RenderPushConstant
{
    int32_t currentFrame;
};


class BasicRenderPipeline
{
public:
    explicit BasicRenderPipeline(const RenderPipelineInfo& pipelineInfo, VulkanContext& context);

    ~BasicRenderPipeline();

    void setupDescriptors(const RenderDescriptorInfo& descriptorInfo);

    void draw(VkCommandBuffer cmd, const RenderDrawInfo& drawInfo) const;

private:
    VulkanContext& context;

    VkDescriptorSetLayout samplerDescriptorLayout{VK_NULL_HANDLE};
    //VkDescriptorSetLayout renderUniformDescriptorSetLayout{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};

    DescriptorBufferSampler samplerDescriptorBuffer;
    //DescriptorBufferUniform uniformDescriptorBuffer;
};
}


#endif //BASIC_RENDER_PIPELINE_H
