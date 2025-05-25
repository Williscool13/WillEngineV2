//
// Created by William on 2025-05-02.
//

#ifndef DEBUG_PIPELINE_H
#define DEBUG_PIPELINE_H

#include <volk/volk.h>

#include "engine/renderer/resources/descriptor_set_layout.h"
#include "engine/renderer/resources/pipeline.h"
#include "engine/renderer/resources/pipeline_layout.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_sampler.h"

namespace will_engine::renderer
{
class ResourceManager;

struct DebugCompositePipelineDrawInfo
{
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
};

class DebugCompositePipeline {
public:
    explicit DebugCompositePipeline(ResourceManager& resourceManager);

    ~DebugCompositePipeline();

    void setupDescriptorBuffer(VkImageView debugTarget, VkImageView finalImageView);

    void draw(VkCommandBuffer cmd, DebugCompositePipelineDrawInfo drawInfo) const;

    void reloadShaders() { createPipeline(); }

private:
    void createPipeline();

private:
    ResourceManager& resourceManager;

    PipelineLayout pipelineLayout{};
    Pipeline pipeline{};
    DescriptorSetLayout descriptorSetLayout{};
    DescriptorBufferSampler descriptorBuffer;
};

}

#endif //DEBUG_PIPELINE_H
