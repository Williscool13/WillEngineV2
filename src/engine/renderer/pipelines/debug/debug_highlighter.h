//
// Created by William on 2025-05-04.
//

#ifndef DEBUG_HIGHLIGHT_PIPELINE_H
#define DEBUG_HIGHLIGHT_PIPELINE_H

#include <vulkan/vulkan_core.h>

#include "engine/renderer/resources/pipeline.h"
#include "engine/renderer/resources/pipeline_layout.h"
#include "engine/renderer/resources/resources_fwd.h"


namespace will_engine::renderer
{
struct DebugHighlighterDrawInfo;
class ResourceManager;

class DebugHighlighter
{
public:
    explicit DebugHighlighter(ResourceManager& resourceManager);

    ~DebugHighlighter();

    void setupDescriptorBuffer(VkImageView stencilImageView, VkImageView debugTarget);

    bool drawHighlightStencil(VkCommandBuffer cmd, const DebugHighlighterDrawInfo& drawInfo) const;

    void drawHighlightProcessing(VkCommandBuffer cmd, const DebugHighlighterDrawInfo& drawInfo) const;

    void reloadShaders() { createPipeline(); }

private:
    void createPipeline();

private:
    ResourceManager& resourceManager;

    PipelineLayoutPtr pipelineLayout{};
    PipelinePtr pipeline{};

    DescriptorSetLayoutPtr processingSetLayout{};
    PipelineLayoutPtr processingPipelineLayout{};
    PipelinePtr processingPipeline{};
    DescriptorBufferSamplerPtr descriptorBuffer{};
};
}


#endif //DEBUG_HIGHLIGHT_PIPELINE_H
