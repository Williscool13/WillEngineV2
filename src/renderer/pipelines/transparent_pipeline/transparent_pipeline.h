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

struct TransparentDrawInfo
{
    VkImageView depthTarget;
    int32_t currentFrameOverlap{0};
    const std::vector<RenderObject*>& renderObjects{};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
};

class TransparentPipeline {

public:
    explicit TransparentPipeline(ResourceManager& resourceManager);

    ~TransparentPipeline();

    void draw(VkCommandBuffer cmd, const TransparentDrawInfo& drawInfo) const;

    void hotReload() { createAccumulationPipeline(); }


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



    void createAccumulationPipeline();

    // todo: remove
    friend void ImguiWrapper::imguiInterface(Engine* engine);
};

}

#endif //TRANSPARENT_PIPELINE_H
