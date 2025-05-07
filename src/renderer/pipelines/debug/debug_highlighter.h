//
// Created by William on 2025-05-04.
//

#ifndef DEBUG_HIGHLIGHT_PIPELINE_H
#define DEBUG_HIGHLIGHT_PIPELINE_H

#include <glm/glm.hpp>

#include "src/core/game_object/renderable.h"
#include "src/renderer/imgui_wrapper.h"
#include "src/renderer/resource_manager.h"

namespace will_engine
{
class IRenderable;
}

namespace will_engine::debug_highlight_pipeline
{
struct DebugHighlightDrawPushConstant
{
    glm::mat4 modelMatrix{1.0f};
    glm::vec4 color{1.0f, 0.647f, 0.0f, 1.0f};
};

struct DebugHighlighterDrawInfo
{
    IRenderable* highlightTarget{nullptr};
    AllocatedImage depthStencilTarget{VK_NULL_HANDLE};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
};

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
