//
// Created by William on 2025-01-25.
//

#ifndef TEMPORAL_ANTIALIASING_PIPELINE_H
#define TEMPORAL_ANTIALIASING_PIPELINE_H

#include <vulkan/vulkan_core.h>

#include "engine/renderer/resources/resources_fwd.h"


namespace will_engine::renderer
{
class ResourceManager;

struct TemporalAntialiasingPushConstants
{
    float blendValue{0.1f};
    int32_t taaDebug{0};
};

struct TemporalAntialiasingDescriptor
{
    VkImageView drawImage{VK_NULL_HANDLE};
    VkImageView historyBuffer{VK_NULL_HANDLE};
    VkImageView depthBuffer{VK_NULL_HANDLE};
    VkImageView velocityBuffer{VK_NULL_HANDLE};
    VkImageView outputTarget{VK_NULL_HANDLE};
    VkSampler sampler{VK_NULL_HANDLE};
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

    PipelineLayoutPtr pipelineLayout{};
    PipelinePtr pipeline{};
    DescriptorSetLayoutPtr descriptorSetLayout{};
    DescriptorBufferSamplerPtr descriptorBuffer{};
};
}


#endif //TEMPORAL_ANTIALIASING_PIPELINE_H
