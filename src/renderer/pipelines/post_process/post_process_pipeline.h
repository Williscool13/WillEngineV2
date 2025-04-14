//
// Created by William on 2025-01-25.
//

#ifndef POST_PROCESS_PIPELINE_H
#define POST_PROCESS_PIPELINE_H

#include <volk/volk.h>

#include "src/renderer/descriptor_buffer/descriptor_buffer_sampler.h"

namespace will_engine
{
class ResourceManager;
}

namespace will_engine::post_process_pipeline
{
struct PostProcessDrawInfo;
struct PostProcessDescriptor;

class PostProcessPipeline
{
public:
    explicit PostProcessPipeline(ResourceManager& resourceManager);

    ~PostProcessPipeline();

    void setupDescriptorBuffer(const PostProcessDescriptor& bufferInfo);

    void draw(VkCommandBuffer cmd, PostProcessDrawInfo drawInfo) const;

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


#endif //POST_PROCESS_PIPELINE_H
