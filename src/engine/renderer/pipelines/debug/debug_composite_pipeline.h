//
// Created by William on 2025-05-02.
//

#ifndef DEBUG_PIPELINE_H
#define DEBUG_PIPELINE_H

#include <vulkan/vulkan_core.h>

#include "engine/renderer/renderer_constants.h"
#include "engine/renderer/resources/resources_fwd.h"

namespace will_engine::renderer
{
class ResourceManager;

struct DebugCompositePipelineDrawInfo
{
    VkExtent2D extents{DEFAULT_RENDER_EXTENT_2D};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
};

class DebugCompositePipeline {
public:
    explicit DebugCompositePipeline(ResourceManager& resourceManager);

    ~DebugCompositePipeline();

    void setupDescriptorBuffer(VkImageView debugTarget, VkImageView finalImageView);

    void draw(VkCommandBuffer cmd, const DebugCompositePipelineDrawInfo& drawInfo) const;

    void reloadShaders() { createPipeline(); }

private:
    void createPipeline();

private:
    ResourceManager& resourceManager;

    PipelineLayoutPtr pipelineLayout{};
    PipelinePtr pipeline{};
    DescriptorSetLayoutPtr descriptorSetLayout{};
    DescriptorBufferSamplerPtr descriptorBuffer{};
};

}

#endif //DEBUG_PIPELINE_H
