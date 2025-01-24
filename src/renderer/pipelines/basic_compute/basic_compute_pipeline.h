//
// Created by William on 2025-01-22.
//

#ifndef BASIC_COMPUTE_PIPELINE_H
#define BASIC_COMPUTE_PIPELINE_H

#include "src/renderer/vulkan_context.h"
#include "src/renderer/descriptor_buffer/descriptor_buffer_sampler.h"


namespace will_engine::basic_compute
{
struct ComputeDescriptorInfo
{
    VkImageView inputImage;
};

struct ComputeDrawInfo
{
    VkExtent2D renderExtent;
};

class BasicComputePipeline
{
public:
    explicit BasicComputePipeline(VulkanContext& context);

    ~BasicComputePipeline();

    void setupDescriptors(const ComputeDescriptorInfo& descriptorInfo);

    void draw(VkCommandBuffer cmd, ComputeDrawInfo drawInfo) const;

private:
    VulkanContext& context;
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
    VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};

    DescriptorBufferSampler samplerDescriptorBuffer;
};
}


#endif //BASIC_COMPUTE_PIPELINE_H
