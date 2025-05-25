//
// Created by William on 2025-03-23.
//

#ifndef GROUND_TRUTH_AMBIENT_OCCLUSION_H
#define GROUND_TRUTH_AMBIENT_OCCLUSION_H

#include <array>

#include "ambient_occlusion_types.h"
#include "engine/renderer/resources/allocated_image.h"
#include "engine/renderer/resources/descriptor_set_layout.h"
#include "engine/renderer/resources/image_view.h"
#include "engine/renderer/resources/pipeline.h"
#include "engine/renderer/resources/pipeline_layout.h"
#include "engine/renderer/resources/sampler.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_sampler.h"


namespace will_engine::renderer
{
class ResourceManager;

class GroundTruthAmbientOcclusionPipeline
{
public:
    explicit GroundTruthAmbientOcclusionPipeline(ResourceManager& resourceManager);

    ~GroundTruthAmbientOcclusionPipeline();

    void setupDepthPrefilterDescriptorBuffer(const VkImageView& depthImageView);

    void setupAmbientOcclusionDescriptorBuffer(const VkImageView& normalsImageView);

    void setupSpatialFilteringDescriptorBuffer();

    void draw(VkCommandBuffer cmd, const GTAODrawInfo& drawInfo);

    void reloadShaders();

    VkImageView getAmbientOcclusionRenderTarget() const { return denoisedFinalAO.imageView; }

private:
    void createDepthPrefilterPipeline();

    void createAmbientOcclusionPipeline();

    void createSpatialFilteringPipeline();

private: // Depth Pre-filter
    DescriptorSetLayout depthPrefilterSetLayout{};
    PipelineLayout depthPrefilterPipelineLayout{};
    Pipeline depthPrefilterPipeline{};

    Sampler depthSampler{};

    // 16 vs 32. look at cost later.
    VkFormat depthPrefilterFormat{VK_FORMAT_R16_SFLOAT};
    AllocatedImage depthPrefilterImage{};
    std::array<ImageView, DEPTH_PREFILTER_MIP_COUNT> depthPrefilterImageViews{};

    DescriptorBufferSampler depthPrefilterDescriptorBuffer;

private: // Ambient Occlusion
    DescriptorSetLayout ambientOcclusionSetLayout{};
    PipelineLayout ambientOcclusionPipelineLayout{};
    Pipeline ambientOcclusionPipeline{};

    Sampler depthPrefilterSampler{};
    Sampler normalsSampler{};

    // 8 is supposedly enough?
    VkFormat ambientOcclusionFormat{VK_FORMAT_R8_UNORM};
    AllocatedImage ambientOcclusionImage{};

    VkFormat edgeDataFormat{VK_FORMAT_R8_UNORM};
    AllocatedImage edgeDataImage{};

    DescriptorBufferSampler ambientOcclusionDescriptorBuffer;

private: // Spatial Filtering
    DescriptorSetLayout spatialFilteringSetLayout{};
    PipelineLayout spatialFilteringPipelineLayout{};
    Pipeline spatialFilteringPipeline{};

    AllocatedImage denoisedFinalAO{};

    DescriptorBufferSampler spatialFilteringDescriptorBuffer;

private: // Debug
    VkFormat debugFormat{VK_FORMAT_R8G8B8A8_UNORM};
    AllocatedImage debugImage{};

private:
    ResourceManager& resourceManager;
};
}


#endif //GROUND_TRUTH_AMBIENT_OCCLUSION_H
