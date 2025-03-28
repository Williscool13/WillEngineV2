//
// Created by William on 2025-01-22.
//

#ifndef BASIC_COMPUTE_PIPELINE_H
#define BASIC_COMPUTE_PIPELINE_H

#include "src/renderer/resource_manager.h"
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
    explicit BasicComputePipeline(ResourceManager& resourceManager);

    ~BasicComputePipeline();

    void setupDescriptors(const ComputeDescriptorInfo& descriptorInfo);

    void draw(VkCommandBuffer cmd, ComputeDrawInfo drawInfo) const;

    void reloadShaders() { createPipeline(); }

private:
    void createPipeline();

private:
    ResourceManager& resourceManager;

    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
    VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};

    DescriptorBufferSampler samplerDescriptorBuffer;
};
}


#endif //BASIC_COMPUTE_PIPELINE_H
