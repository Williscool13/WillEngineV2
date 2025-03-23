//
// Created by William on 2025-03-23.
//

#ifndef GROUND_TRUTH_AMBIENT_OCCLUSION_H
#define GROUND_TRUTH_AMBIENT_OCCLUSION_H

#include <array>
#include <volk/volk.h>

#include "src/renderer/resource_manager.h"
#include "src/renderer/lighting/ambient_occlusion/ambient_occlusion_types.h"


namespace will_engine::ambient_occlusion
{
class GroundTruthAmbientOcclusionPipeline
{
public:
    GroundTruthAmbientOcclusionPipeline(ResourceManager& resourceManager);

    ~GroundTruthAmbientOcclusionPipeline();

    void setupDepthPrefilterDescriptorBuffer(VkSampler depthImageSampler, VkImageView depthImageView);

private:
    void createDepthPrefilterPipeline();

private:
    VkDescriptorSetLayout depthPrefilterSetLayout{VK_NULL_HANDLE};
    VkPipelineLayout depthPrefilterPipelineLayout{VK_NULL_HANDLE};
    VkPipeline depthPrefilterPipeline{VK_NULL_HANDLE};

    // 16 vs 32. look at cost later.
    VkFormat depthPrefilterFormat{VK_FORMAT_R16_SFLOAT};
    AllocatedImage depthPrefilterImage{VK_NULL_HANDLE};
    std::array<VkImageView, DEPTH_PREFILTER_MIP_COUNT> depthPrefilterImageViews{};

    DescriptorBufferSampler depthPrefilterDescriptorBuffer;

    VkPipelineLayout ambientOcclusionPipelineLayout{VK_NULL_HANDLE};
    VkPipeline ambientOcclusionPipeline{VK_NULL_HANDLE};

    VkPipelineLayout spatialFilteringPipelineLayout{VK_NULL_HANDLE};
    VkPipeline spatialFilteringPipeline{VK_NULL_HANDLE};

    VkPipelineLayout temporalAccumulationPipelineLayout{VK_NULL_HANDLE};
    VkPipeline temporalAccumulationPipeline{VK_NULL_HANDLE};

private:
    ResourceManager& resourceManager;
};
}


#endif //GROUND_TRUTH_AMBIENT_OCCLUSION_H
