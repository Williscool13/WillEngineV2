//
// Created by William on 2025-01-25.
//

#ifndef TEMPORAL_ANTIALIASING_PIPELINE_H
#define TEMPORAL_ANTIALIASING_PIPELINE_H

#include <volk/volk.h>

#include "temporal_antialiasing_pipeline_types.h"
#include "src/renderer/descriptor_buffer/descriptor_buffer_sampler.h"

namespace will_engine
{
class ResourceManager;
}

namespace will_engine::temporal_antialiasing_pipeline
{
class TemporalAntialiasingPipeline
{
public:
    explicit TemporalAntialiasingPipeline(ResourceManager& resourceManager);

    ~TemporalAntialiasingPipeline();

    void setupDescriptorBuffer(const TemporalAntialiasingDescriptor& descriptor);

    void draw(VkCommandBuffer cmd, const TemporalAntialiasingDrawInfo& drawInfo) const;

    void reloadShaders() { createPipeline(); }

private:
    void createPipeline();

private:
    ResourceManager& resourceManager;

    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
    VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
    DescriptorBufferSampler descriptorBuffer;
};
}


#endif //TEMPORAL_ANTIALIASING_PIPELINE_H
