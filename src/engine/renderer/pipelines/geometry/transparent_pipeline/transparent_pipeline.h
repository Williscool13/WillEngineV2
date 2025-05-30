//
// Created by William on 2025-04-06.
//

#ifndef TRANSPARENT_PIPELINE_H
#define TRANSPARENT_PIPELINE_H

#include <vector>
#include <vulkan/vulkan_core.h>

#include "engine/renderer/resources/resources_fwd.h"

namespace will_engine::renderer
{
class RenderObject;
class ResourceManager;

struct TransparentPushConstants
{
    int32_t bEnabled{true};
    int32_t bDisableShadows{false};
    int32_t bDisableContactShadows{false};
};

struct TransparentAccumulateDrawInfo
{
    bool enabled{true};
    VkImageView depthTarget{VK_NULL_HANDLE};
    int32_t currentFrameOverlap{0};
    const std::vector<RenderObject*>& renderObjects{};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
    VkDescriptorBufferBindingInfoEXT environmentIBLBinding{};
    VkDeviceSize environmentIBLOffset{0};
    VkDescriptorBufferBindingInfoEXT cascadeUniformBinding{};
    VkDeviceSize cascadeUniformOffset{0};
    VkDescriptorBufferBindingInfoEXT cascadeSamplerBinding{};
};

struct TransparentCompositeDrawInfo
{
    VkImageView opaqueImage;
};

class TransparentPipeline
{
public:
    explicit TransparentPipeline(ResourceManager& resourceManager, VkDescriptorSetLayout environmentIBLLayout,
                                 VkDescriptorSetLayout cascadeUniformLayout, VkDescriptorSetLayout cascadeSamplerLayout);

    ~TransparentPipeline();

    void drawAccumulate(VkCommandBuffer cmd, const TransparentAccumulateDrawInfo& drawInfo) const;

    void drawComposite(VkCommandBuffer cmd, const TransparentCompositeDrawInfo& drawInfo) const;

    void reloadShaders()
    {
        createAccumulationPipeline();
        createCompositePipeline();
    }

private:
    ResourceManager& resourceManager;

    const VkFormat debugImageFormat{VK_FORMAT_R16G16B16A16_SFLOAT};
    ImageResourcePtr debugImage{};

    const VkFormat accumulationImageFormat{VK_FORMAT_R16G16B16A16_SFLOAT};
    ImageResourcePtr accumulationImage{};

    PipelineLayoutPtr accumulationPipelineLayout{};
    PipelinePtr accumulationPipeline{};

    const VkFormat revealageImageFormat{VK_FORMAT_R16_SFLOAT};
    ImageResourcePtr revealageImage{};

    DescriptorSetLayoutPtr compositeDescriptorSetLayout{};
    PipelineLayoutPtr compositePipelineLayout{};
    PipelinePtr compositePipeline{};
    DescriptorBufferSamplerPtr compositeDescriptorBuffer;

    void createAccumulationPipeline();

    void createCompositePipeline();

};
}

#endif //TRANSPARENT_PIPELINE_H
