//
// Created by William on 2025-01-22.
//

#ifndef BASIC_RENDER_PIPELINE_H
#define BASIC_RENDER_PIPELINE_H

#include <volk/volk.h>

#include "engine/renderer/resources/descriptor_set_layout.h"
#include "engine/renderer/resources/pipeline.h"
#include "engine/renderer/resources/pipeline_layout.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_sampler.h"


namespace will_engine::renderer
{
class ResourceManager;

struct RenderDescriptorInfo
{
    VkSampler sampler{VK_NULL_HANDLE};
    VkImageView texture{VK_NULL_HANDLE};
};

struct RenderDrawInfo
{
    VkExtent2D renderExtent{};
    VkImageView drawImage{VK_NULL_HANDLE};
    VkImageView depthImage{VK_NULL_HANDLE};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
    int32_t currentFrame{};
};

struct RenderPushConstant
{
    int32_t currentFrame;
};

class BasicRenderPipeline
{
public:
    explicit BasicRenderPipeline(ResourceManager& resourceManager);

    ~BasicRenderPipeline();

    void setupDescriptors(const RenderDescriptorInfo& descriptorInfo);

    void draw(VkCommandBuffer cmd, const RenderDrawInfo& drawInfo) const;

    void reloadShaders() { createPipeline(); }

private:
    void createPipeline();

private:
    ResourceManager& resourceManager;

    DescriptorSetLayout samplerDescriptorLayout{};
    PipelineLayout pipelineLayout{};
    Pipeline pipeline{};
    DescriptorBufferSampler samplerDescriptorBuffer;
};
}


#endif //BASIC_RENDER_PIPELINE_H
