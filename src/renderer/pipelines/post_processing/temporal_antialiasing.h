//
// Created by William on 2024-12-07.
//

#ifndef TEMPORAL_ANTIALIASING_H
#define TEMPORAL_ANTIALIASING_H

#include <vulkan/vulkan_core.h>
#include "src/renderer/vk_descriptor_buffer.h"
#include "src/renderer/vulkan_context.h"
#include "src/core/camera/camera.h"

struct TaaDescriptorBufferInfo
{
    VkImageView drawImage;
    VkImageView historyBuffer;
    VkImageView depthBuffer;
    VkImageView velocityBuffer;
    VkImageView outputTarget;
    VkSampler sampler;
};

struct TaaDrawInfo
{
    VkExtent2D renderExtent;
    float blendValue;
    float velocityWeight;
    bool enabled;
    int32_t debugMode;
};

class TaaPipeline
{
public:
    explicit TaaPipeline(VulkanContext& context);

    ~TaaPipeline();

    void init();

    void draw(VkCommandBuffer cmd, const TaaDrawInfo& drawInfo) const;

    void setupDescriptorBuffer(const TaaDescriptorBufferInfo& bufferInfo);

private:
    VulkanContext& context;
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
    VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};

    DescriptorBufferSampler descriptorBuffer;

    void createDescriptorLayout();

    void createPipelineLayout();

    void createPipeline();

    void cleanup();
};
#endif //TEMPORAL_ANTIALIASING_H
