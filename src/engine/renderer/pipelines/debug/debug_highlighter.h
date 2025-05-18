//
// Created by William on 2025-05-04.
//

#ifndef DEBUG_HIGHLIGHT_PIPELINE_H
#define DEBUG_HIGHLIGHT_PIPELINE_H

#include <glm/glm.hpp>

#include "engine/renderer/pipelines/debug/debug_highlight_types.h"
#include "engine/core/game_object/renderable.h"
#include "engine/renderer/imgui_wrapper.h"
#include "engine/renderer/resource_manager.h"

namespace will_engine
{
class IRenderable;
}

namespace will_engine::debug_highlight_pipeline
{

class DebugHighlighter {
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

    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};

    VkDescriptorSetLayout processingSetLayout{VK_NULL_HANDLE};
    VkPipelineLayout processingPipelineLayout{VK_NULL_HANDLE};
    VkPipeline processingPipeline{VK_NULL_HANDLE};
    DescriptorBufferSampler descriptorBuffer{};

    // todo: remove
    friend void ImguiWrapper::imguiInterface(Engine* engine);
};
}





#endif //DEBUG_HIGHLIGHT_PIPELINE_H
