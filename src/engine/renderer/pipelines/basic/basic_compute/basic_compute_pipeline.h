//
// Created by William on 2025-01-22.
//

#ifndef BASIC_COMPUTE_PIPELINE_H
#define BASIC_COMPUTE_PIPELINE_H

#include "engine/renderer/resources/descriptor_set_layout.h"
#include "engine/renderer/resources/pipeline.h"
#include "engine/renderer/resources/pipeline_layout.h"
#include "engine/renderer/resources/resources_fwd.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_sampler.h"


namespace will_engine::renderer
{
class ResourceManager;
}

namespace will_engine::renderer
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

    PipelineLayoutPtr pipelineLayout{};
    PipelinePtr pipeline{};
    DescriptorSetLayoutPtr descriptorSetLayout{};
    DescriptorBufferSamplerPtr samplerDescriptorBuffer{};
};
}


#endif //BASIC_COMPUTE_PIPELINE_H
