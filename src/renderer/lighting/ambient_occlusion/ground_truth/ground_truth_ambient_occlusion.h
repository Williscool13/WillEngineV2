//
// Created by William on 2025-03-23.
//

#ifndef GROUND_TRUTH_AMBIENT_OCCLUSION_H
#define GROUND_TRUTH_AMBIENT_OCCLUSION_H

#include <array>

#include "src/renderer/resource_manager.h"
#include "src/renderer/lighting/ambient_occlusion/ambient_occlusion_types.h"


namespace will_engine::ambient_occlusion
{
class GroundTruthAmbientOcclusionPipeline
{
public:
    explicit GroundTruthAmbientOcclusionPipeline(ResourceManager& resourceManager);

    ~GroundTruthAmbientOcclusionPipeline();

    void setupDepthPrefilterDescriptorBuffer(VkImageView depthImageView);

    void setupAmbientOcclusionDescriptorBuffer(VkImageView normalsImageView);

    void draw(VkCommandBuffer cmd, const GTAODrawInfo& drawInfo) const;

private:
    void createDepthPrefilterPipeline();
    void createAmbientOcclusionPipeline();
    void createSpatialFilteringPipeline();
    void createTemporalAccumulationPipeline();

private: // Depth Pre-filter
    VkDescriptorSetLayout depthPrefilterSetLayout{VK_NULL_HANDLE};
    VkPipelineLayout depthPrefilterPipelineLayout{VK_NULL_HANDLE};
    VkPipeline depthPrefilterPipeline{VK_NULL_HANDLE};

    VkSampler depthPrefilterSampler{VK_NULL_HANDLE};

    // 16 vs 32. look at cost later.
    VkFormat depthPrefilterFormat{VK_FORMAT_R16_SFLOAT};
    AllocatedImage depthPrefilterImage{VK_NULL_HANDLE};
    std::array<VkImageView, DEPTH_PREFILTER_MIP_COUNT> depthPrefilterImageViews{};

    DescriptorBufferSampler depthPrefilterDescriptorBuffer;

private: // Ambient Occlusion
    VkDescriptorSetLayout ambientOcclusionSetLayout{VK_NULL_HANDLE};
    VkPipelineLayout ambientOcclusionPipelineLayout{VK_NULL_HANDLE};
    VkPipeline ambientOcclusionPipeline{VK_NULL_HANDLE};

    VkSampler ambientOcclusionDepthSampler{VK_NULL_HANDLE};
    VkSampler ambientOcclusionNormalsSampler{VK_NULL_HANDLE};

    // 8 is supposedly enough?
    VkFormat ambientOcclusionFormat{VK_FORMAT_R8_UNORM};
    AllocatedImage ambientOcclusionImage{VK_NULL_HANDLE};

    DescriptorBufferSampler ambientOcclusionDescriptorBuffer;

private: // Spatial Filtering
    VkDescriptorSetLayout spatialFilteringSetLayout{VK_NULL_HANDLE};
    VkPipelineLayout spatialFilteringPipelineLayout{VK_NULL_HANDLE};
    VkPipeline spatialFilteringPipeline{VK_NULL_HANDLE};

    AllocatedImage spatialFilteringImage{VK_NULL_HANDLE};

    DescriptorBufferSampler spatialFilteringDescriptorBuffer;

private: // Temporal Accumulation
    VkDescriptorSetLayout temporalAccumulationSetLayout{VK_NULL_HANDLE};
    VkPipelineLayout temporalAccumulationPipelineLayout{VK_NULL_HANDLE};
    VkPipeline temporalAccumulationPipeline{VK_NULL_HANDLE};

    AllocatedImage historyOutputImage{VK_NULL_HANDLE};

    DescriptorBufferSampler temporalAccumulationDescriptorBuffer;

private: // Output
    AllocatedImage ambientOcclusionOutputImage{VK_NULL_HANDLE};

private:
    ResourceManager& resourceManager;
};
}


#endif //GROUND_TRUTH_AMBIENT_OCCLUSION_H
