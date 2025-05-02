//
// Created by William on 2025-05-02.
//

#ifndef DEBUG_PIPELINE_H
#define DEBUG_PIPELINE_H


#ifndef WILL_ENGINE_DEBUG
    #error This file should only be included when JPH_DEBUG_RENDERER is defined
#endif // !WILL_ENGINE_DEBUG

#include "src/renderer/resource_manager.h"


namespace will_engine::debug_pipeline
{

struct DebugPushConstant
{
    glm::vec2 renderBounds{RENDER_EXTENT_WIDTH, RENDER_EXTENT_HEIGHT};
};

class DebugPipeline {
public:
    explicit DebugPipeline(ResourceManager& resourceManager);

    ~DebugPipeline();

    void setupDescriptorBuffer(VkImageView finalImageView);

    void draw(VkCommandBuffer cmd) const;

    void reloadShaders() { createPipeline(); }

    const AllocatedImage& getDebugTarget() const { return debugTarget; }

private:
    void createPipeline();

private:
    ResourceManager& resourceManager;

    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
    VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
    DescriptorBufferSampler descriptorBuffer;

    AllocatedImage debugTarget{};
};

}

#endif //DEBUG_PIPELINE_H
