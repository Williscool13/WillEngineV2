//
// Created by William on 2025-01-22.
//

#ifndef BASIC_RENDER_PIPELINE_H
#define BASIC_RENDER_PIPELINE_H

#include <volk/volk.h>

#include "src/renderer/descriptor_buffer/descriptor_buffer_sampler.h"

namespace will_engine
{
class ResourceManager;
}

namespace will_engine::basic_render_pipeline
{
struct RenderDrawInfo;
struct RenderDescriptorInfo;

class BasicRenderPipeline
{
public:
    explicit BasicRenderPipeline(ResourceManager& resourceManager);

    ~BasicRenderPipeline();

    void setupDescriptors(const RenderDescriptorInfo& descriptorInfo);

    void draw(VkCommandBuffer cmd, const RenderDrawInfo& drawInfo) const;

    void reloadShaders() { createPipeline(); }

private:
    void createPipeline();

private:
    ResourceManager& resourceManager;

    VkDescriptorSetLayout samplerDescriptorLayout{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};

    DescriptorBufferSampler samplerDescriptorBuffer;
};
}


#endif //BASIC_RENDER_PIPELINE_H
