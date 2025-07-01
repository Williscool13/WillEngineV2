//
// Created by William on 2025-03-23.
//

#ifndef GROUND_TRUTH_AMBIENT_OCCLUSION_H
#define GROUND_TRUTH_AMBIENT_OCCLUSION_H

#include <array>

#include "ambient_occlusion_types.h"
#include "engine/renderer/render_context.h"
#include "engine/renderer/resources/image_resource.h"
#include "engine/renderer/resources/resources_fwd.h"


namespace will_engine::renderer
{
class ResourceManager;

class GroundTruthAmbientOcclusionPipeline
{
public:
    explicit GroundTruthAmbientOcclusionPipeline(ResourceManager& resourceManager, RenderContext& renderContext);

    ~GroundTruthAmbientOcclusionPipeline();

    void setupDepthPrefilterDescriptorBuffer(const VkImageView& depthImageView);

    void setupAmbientOcclusionDescriptorBuffer(const VkImageView& normalsImageView);

    void setupSpatialFilteringDescriptorBuffer();

    void draw(VkCommandBuffer cmd, const GTAODrawInfo& drawInfo);

    void reloadShaders();

    VkImageView getAmbientOcclusionRenderTarget() const { return denoisedFinalAOImage->imageView; }

private:
    void createDepthPrefilterPipeline();

    void createAmbientOcclusionPipeline();

    void createSpatialFilteringPipeline();

    void createIntermediateRenderTargets(VkExtent2D extents);

    void handleResize(const ResolutionChangedEvent& event);

    EventDispatcher<ResolutionChangedEvent>::Handle resolutionChangedHandle;

private: // Depth Pre-filter
    DescriptorSetLayoutPtr depthPrefilterSetLayout{};
    PipelineLayoutPtr depthPrefilterPipelineLayout{};
    PipelinePtr depthPrefilterPipeline{};

    SamplerPtr depthSampler{};

    // 16 vs 32. look at cost later.
    VkFormat depthPrefilterFormat{VK_FORMAT_R16_SFLOAT};
    ImageResourcePtr depthPrefilterImage{};
    std::array<ImageViewPtr, DEPTH_PREFILTER_MIP_COUNT> depthPrefilterImageViews{};

    DescriptorBufferSamplerPtr depthPrefilterDescriptorBuffer;

private: // Ambient Occlusion
    DescriptorSetLayoutPtr ambientOcclusionSetLayout{};
    PipelineLayoutPtr ambientOcclusionPipelineLayout{};
    PipelinePtr ambientOcclusionPipeline{};

    SamplerPtr depthPrefilterSampler{};
    SamplerPtr normalsSampler{};

    // 8 is supposedly enough?
    VkFormat ambientOcclusionFormat{VK_FORMAT_R8_UNORM};
    ImageResourcePtr ambientOcclusionImage{};

    VkFormat edgeDataFormat{VK_FORMAT_R8_UNORM};
    ImageResourcePtr edgeDataImage{};

    DescriptorBufferSamplerPtr ambientOcclusionDescriptorBuffer;

private: // Spatial Filtering
    DescriptorSetLayoutPtr spatialFilteringSetLayout{};
    PipelineLayoutPtr spatialFilteringPipelineLayout{};
    PipelinePtr spatialFilteringPipeline{};

    ImageResourcePtr denoisedFinalAOImage{};

    DescriptorBufferSamplerPtr spatialFilteringDescriptorBuffer;

private: // Debug
    VkFormat debugFormat{VK_FORMAT_R8G8B8A8_UNORM};
    ImageResourcePtr debugImage{};

private:
    ResourceManager& resourceManager;
};
}


#endif //GROUND_TRUTH_AMBIENT_OCCLUSION_H
