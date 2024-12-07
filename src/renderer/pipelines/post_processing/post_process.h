//
// Created by William on 2024-12-07.
//

#ifndef POST_PROCESS_H
#define POST_PROCESS_H

#include <vulkan/vulkan_core.h>
#include "src/renderer/vk_descriptor_buffer.h"
#include "src/renderer/vulkan_context.h"

struct PostProcessPipelineCreateInfo
{
    VkFormat inputFormat;
    VkFormat outputFormat;
};

struct PostProcessDescriptorBufferInfo
{
    VkImageView inputImage;
    VkImageView outputImage;
    VkSampler nearestSampler;
};

struct PostProcessDrawInfo
{
    VkExtent2D renderExtent;
    bool enabled;
};

class PostProcessPipeline
{
public:
    explicit PostProcessPipeline(VulkanContext& context);

    ~PostProcessPipeline();

    void init(const PostProcessPipelineCreateInfo& createInfo);

    void draw(VkCommandBuffer cmd, const PostProcessDrawInfo& drawInfo) const;

    void setupDescriptorBuffer(const PostProcessDescriptorBufferInfo& bufferInfo);

private:
    VulkanContext& context;
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
    VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};

    DescriptorBufferSampler descriptorBuffer;

    void createDescriptorLayout();

    void createPipelineLayout();

    void createPipeline();

    void cleanup();
};

#endif //POST_PROCESS_H
