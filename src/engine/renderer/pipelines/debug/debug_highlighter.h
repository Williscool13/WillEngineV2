//
// Created by William on 2025-05-04.
//

#ifndef DEBUG_HIGHLIGHT_PIPELINE_H
#define DEBUG_HIGHLIGHT_PIPELINE_H
#include "engine/renderer/resources/descriptor_set_layout.h"
#include "engine/renderer/resources/pipeline.h"
#include "engine/renderer/resources/pipeline_layout.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_sampler.h"


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

    PipelineLayout pipelineLayout{};
    Pipeline pipeline{};

    DescriptorSetLayout processingSetLayout{};
    PipelineLayout processingPipelineLayout{};
    Pipeline processingPipeline{};
    DescriptorBufferSampler descriptorBuffer{};
};
}


#endif //DEBUG_HIGHLIGHT_PIPELINE_H
