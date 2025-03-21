//
// Created by William on 2025-01-22.
//

#ifndef BASIC_RENDER_PIPELINE_H
#define BASIC_RENDER_PIPELINE_H
#include "src/renderer/resource_manager.h"
#include "src/renderer/vulkan_context.h"
#include "src/renderer/descriptor_buffer/descriptor_buffer_sampler.h"


namespace will_engine::basic_render
{
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

    VkDescriptorSetLayout samplerDescriptorLayout{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};

    DescriptorBufferSampler samplerDescriptorBuffer;
};
}


#endif //BASIC_RENDER_PIPELINE_H
