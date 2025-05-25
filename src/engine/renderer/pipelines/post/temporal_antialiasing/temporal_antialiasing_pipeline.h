//
// Created by William on 2025-01-25.
//

#ifndef TEMPORAL_ANTIALIASING_PIPELINE_H
#define TEMPORAL_ANTIALIASING_PIPELINE_H

#include <volk/volk.h>

#include "engine/renderer/resources/descriptor_set_layout.h"
#include "engine/renderer/resources/pipeline.h"
#include "engine/renderer/resources/pipeline_layout.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_sampler.h"

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

    PipelineLayout pipelineLayout{};
    Pipeline pipeline{};
    DescriptorSetLayout descriptorSetLayout{};
    DescriptorBufferSampler descriptorBuffer{};
};
}


#endif //TEMPORAL_ANTIALIASING_PIPELINE_H
