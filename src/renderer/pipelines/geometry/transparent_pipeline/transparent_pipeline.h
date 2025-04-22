//
// Created by William on 2025-04-06.
//

#ifndef TRANSPARENT_PIPELINE_H
#define TRANSPARENT_PIPELINE_H

#include <volk/volk.h>

#include "src/renderer/vk_types.h"
#include "src/renderer/descriptor_buffer/descriptor_buffer_sampler.h"

namespace will_engine
{
class ResourceManager;
class RenderObject;
}

namespace will_engine::transparent_pipeline
{
struct TransparentCompositeDrawInfo;
struct TransparentAccumulateDrawInfo;

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

    VkPipelineLayout accumulationPipelineLayout{VK_NULL_HANDLE};
    VkPipeline accumulationPipeline{VK_NULL_HANDLE};

    const VkFormat revealageImageFormat{VK_FORMAT_R16_SFLOAT};
    AllocatedImage revealageImage{};

    VkDescriptorSetLayout compositeDescriptorSetLayout{VK_NULL_HANDLE};
    VkPipelineLayout compositePipelineLayout{VK_NULL_HANDLE};
    VkPipeline compositePipeline{VK_NULL_HANDLE};
    DescriptorBufferSampler compositeDescriptorBuffer;

    void createAccumulationPipeline();

    void createCompositePipeline();

};
}

#endif //TRANSPARENT_PIPELINE_H
