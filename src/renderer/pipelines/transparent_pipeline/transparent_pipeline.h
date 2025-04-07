//
// Created by William on 2025-04-06.
//

#ifndef TRANSPARENT_PIPELINE_H
#define TRANSPARENT_PIPELINE_H
#include "src/renderer/imgui_wrapper.h"
#include "src/renderer/resource_manager.h"

namespace will_engine
{
class RenderObject;
}

namespace will_engine::transparent_pipeline
{
struct TransparentsPushConstants
{
    int32_t bEnabled;
    int32_t bReceivesShadows{true};
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

    // todo: remove
    friend void ImguiWrapper::imguiInterface(Engine* engine);
};
}

#endif //TRANSPARENT_PIPELINE_H
