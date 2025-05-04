//
// Created by William on 2025-05-04.
//

#ifndef DEBUG_HIGHLIGHT_PIPELINE_H
#define DEBUG_HIGHLIGHT_PIPELINE_H

#include <glm/glm.hpp>

#include "src/renderer/resource_manager.h"

namespace will_engine::debug_highlight_pipeline
{
struct DebugHighlightDrawPushConstant
{
    glm::vec4 modelMatrix{1.0f};
};

class DebugHighlightPipeline {
    explicit DebugHighlightPipeline(ResourceManager& resourceManager);

    ~DebugHighlightPipeline();

    void draw(VkCommandBuffer cmd) const;

    void reloadShaders() { createPipeline(); }

private:
    void createPipeline();

private:
    ResourceManager& resourceManager;

    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};

    AllocatedImage debugHighlightStencil{};
};
}





#endif //DEBUG_HIGHLIGHT_PIPELINE_H
