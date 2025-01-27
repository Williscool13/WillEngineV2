//
// Created by William on 2025-01-25.
//

#ifndef TEMPORAL_ANTIALIASING_PIPELINE_H
#define TEMPORAL_ANTIALIASING_PIPELINE_H
#include "src/renderer/resource_manager.h"


namespace will_engine::temporal_antialiasing_pipeline
{
struct TemporalAntialiasingPushConstants
{
    glm::vec2 texelSize;
    int32_t width;
    int32_t height;
    float blendValue;
    int32_t taaDebug;
};

struct TemporalAntialiasingDescriptor
{
    VkImageView drawImage;
    VkImageView historyBuffer;
    VkImageView depthBuffer;
    VkImageView velocityBuffer;
    VkImageView outputTarget;
    VkSampler sampler;
};

struct TemporalAntialiasingDrawInfo
{
    float blendValue{};
    int32_t debugMode{};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
};

class TemporalAntialiasingPipeline
{
public:
    explicit TemporalAntialiasingPipeline(ResourceManager& resourceManager);

    ~TemporalAntialiasingPipeline();

    void setupDescriptorBuffer(const TemporalAntialiasingDescriptor& descriptor);

    void draw(VkCommandBuffer cmd, const TemporalAntialiasingDrawInfo& drawInfo) const;

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


#endif //TEMPORAL_ANTIALIASING_PIPELINE_H
