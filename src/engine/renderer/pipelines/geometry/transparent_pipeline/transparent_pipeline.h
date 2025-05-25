//
// Created by William on 2025-04-06.
//

#ifndef TRANSPARENT_PIPELINE_H
#define TRANSPARENT_PIPELINE_H

#include <volk/volk.h>

#include "engine/renderer/vk_types.h"
#include "engine/renderer/resources/allocated_image.h"
#include "engine/renderer/resources/descriptor_set_layout.h"
#include "engine/renderer/resources/pipeline.h"
#include "engine/renderer/resources/pipeline_layout.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_sampler.h"

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
    AllocatedImage debugImage{};

    const VkFormat accumulationImageFormat{VK_FORMAT_R16G16B16A16_SFLOAT};
    AllocatedImage accumulationImage{};

    PipelineLayout accumulationPipelineLayout{};
    Pipeline accumulationPipeline{};

    const VkFormat revealageImageFormat{VK_FORMAT_R16_SFLOAT};
    AllocatedImage revealageImage{};

    DescriptorSetLayout compositeDescriptorSetLayout{};
    PipelineLayout compositePipelineLayout{};
    Pipeline compositePipeline{};
    DescriptorBufferSampler compositeDescriptorBuffer;

    void createAccumulationPipeline();

    void createCompositePipeline();

};
}

#endif //TRANSPARENT_PIPELINE_H
