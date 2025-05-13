//
// Created by William on 2025-01-22.
//

#ifndef BASIC_COMPUTE_PIPELINE_H
#define BASIC_COMPUTE_PIPELINE_H

#include <volk/volk.h>

#include "engine/renderer/descriptor_buffer/descriptor_buffer_sampler.h"

namespace will_engine
{
class ResourceManager;
}

namespace will_engine::basic_compute_pipeline
{
struct ComputeDrawInfo;
struct ComputeDescriptorInfo;

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
