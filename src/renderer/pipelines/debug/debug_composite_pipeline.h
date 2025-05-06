//
// Created by William on 2025-05-02.
//

#ifndef DEBUG_PIPELINE_H
#define DEBUG_PIPELINE_H


#ifndef WILL_ENGINE_DEBUG
    #error This file should only be included when WILL_ENGINE_DEBUG is defined
#endif // !WILL_ENGINE_DEBUG

#include "src/renderer/resource_manager.h"


namespace will_engine::debug_pipeline
{
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

    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
    VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
    DescriptorBufferSampler descriptorBuffer;
};

}

#endif //DEBUG_PIPELINE_H
