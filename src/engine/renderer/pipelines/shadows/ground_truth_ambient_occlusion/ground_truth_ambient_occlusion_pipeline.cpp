//
// Created by William on 2025-03-23.
//

#include "ground_truth_ambient_occlusion_pipeline.h"

#include <volk/volk.h>

#include "engine/renderer/renderer_constants.h"
#include "engine/renderer/vk_descriptors.h"
#include "engine/renderer/resources/image.h"
#include "engine/renderer/resource_manager.h"
#include "engine/renderer/vk_helpers.h"
#include "ambient_occlusion_types.h"
#include "engine/renderer/resources/image_view.h"
#include "engine/renderer/resources/pipeline.h"
#include "engine/renderer/resources/pipeline_layout.h"
#include "engine/renderer/resources/shader_module.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_sampler.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_types.h"

namespace will_engine::renderer
{
GroundTruthAmbientOcclusionPipeline::GroundTruthAmbientOcclusionPipeline(
    ResourceManager& resourceManager) : resourceManager(resourceManager)
{
    // Debug
    {
        VkImageUsageFlags usage{};
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        VkImageCreateInfo imgInfo = vk_helpers::imageCreateInfo(debugFormat, usage, {RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1});
        debugImage = resourceManager.createResource<Image>(imgInfo);
    }

    // Depth Pre-filtering
    {
        DescriptorLayoutBuilder layoutBuilder{6};
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // MRT depth buffer
        layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ao depth mip 0
        layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ao depth mip 1
        layoutBuilder.addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ao depth mip 2
        layoutBuilder.addBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ao depth mip 3
        layoutBuilder.addBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ao depth mip 4
        layoutBuilder.addBinding(6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // debug image
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = layoutBuilder.build(VK_SHADER_STAGE_COMPUTE_BIT,
                                                                               VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
        depthPrefilterSetLayout = resourceManager.createResource<DescriptorSetLayout>(layoutCreateInfo);

        VkPushConstantRange pushConstants{};
        pushConstants.offset = 0;
        pushConstants.size = sizeof(GTAOPushConstants);
        pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        std::array setLayouts{
            resourceManager.getSceneDataLayout(),
            depthPrefilterSetLayout->layout,
        };


        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = nullptr;
        layoutInfo.pSetLayouts = setLayouts.data();
        layoutInfo.setLayoutCount = setLayouts.size();
        layoutInfo.pPushConstantRanges = &pushConstants;
        layoutInfo.pushConstantRangeCount = 1;

        depthPrefilterPipelineLayout = resourceManager.createResource<PipelineLayout>(layoutInfo);
        createDepthPrefilterPipeline();

        depthPrefilterDescriptorBuffer = resourceManager.createResource<DescriptorBufferSampler>(depthPrefilterSetLayout->layout, 1);

        VkImageUsageFlags usage{};
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        VkImageCreateInfo imgInfo = vk_helpers::imageCreateInfo(depthPrefilterFormat, usage, {RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1});
        // 5 mips, suggested by Intel's implementation
        // https://github.com/GameTechDev/XeGTAO
        imgInfo.mipLevels = DEPTH_PREFILTER_MIP_COUNT;
        depthPrefilterImage = resourceManager.createResource<Image>(imgInfo);
        VkImageViewCreateInfo viewInfo = vk_helpers::imageviewCreateInfo(depthPrefilterFormat, depthPrefilterImage->image, VK_IMAGE_ASPECT_COLOR_BIT);

        for (int32_t i = 0; i < DEPTH_PREFILTER_MIP_COUNT; ++i) {
            viewInfo.subresourceRange.baseMipLevel = i;
            depthPrefilterImageViews[i] = resourceManager.createResource<ImageView>(viewInfo);
        }

        VkSamplerCreateInfo samplerInfo = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        depthSampler = resourceManager.createResource<Sampler>(samplerInfo);
    }

    // AO Calculation
    {
        DescriptorLayoutBuilder layoutBuilder{5};
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // pre-filtered depth
        layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // MRT normal buffer
        layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ao output
        layoutBuilder.addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // edge data output
        layoutBuilder.addBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // debug image
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = layoutBuilder.build(
            VK_SHADER_STAGE_COMPUTE_BIT,
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );

        ambientOcclusionSetLayout = resourceManager.createResource<DescriptorSetLayout>(layoutCreateInfo);

        VkPushConstantRange pushConstants{};
        pushConstants.offset = 0;
        pushConstants.size = sizeof(GTAOPushConstants);
        pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        std::array setLayouts{
            resourceManager.getSceneDataLayout(),
            ambientOcclusionSetLayout->layout,
        };


        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = nullptr;
        layoutInfo.pSetLayouts = setLayouts.data();
        layoutInfo.setLayoutCount = setLayouts.size();
        layoutInfo.pPushConstantRanges = &pushConstants;
        layoutInfo.pushConstantRangeCount = 1;

        ambientOcclusionPipelineLayout = resourceManager.createResource<PipelineLayout>(layoutInfo);
        createAmbientOcclusionPipeline();

        ambientOcclusionDescriptorBuffer = resourceManager.createResource<DescriptorBufferSampler>(ambientOcclusionSetLayout->layout, 1);

        VkImageUsageFlags usage{};
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

        VkImageCreateInfo imgInfo = vk_helpers::imageCreateInfo(ambientOcclusionFormat, usage, {RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1});
        ambientOcclusionImage = resourceManager.createResource<Image>(imgInfo);

        usage = {};
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

        imgInfo = vk_helpers::imageCreateInfo(edgeDataFormat, usage, {RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1});
        edgeDataImage = resourceManager.createResource<Image>(imgInfo);


        // Depth Mip sampler
        {
            VkSamplerCreateInfo samplerInfo = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
            samplerInfo.magFilter = VK_FILTER_NEAREST;
            samplerInfo.minFilter = VK_FILTER_NEAREST;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.anisotropyEnable = VK_FALSE;
            samplerInfo.maxAnisotropy = 1.0f;
            samplerInfo.compareEnable = VK_FALSE;
            samplerInfo.minLod = 0.0f;
            samplerInfo.maxLod = DEPTH_PREFILTER_MIP_COUNT - 1;

            depthPrefilterSampler = resourceManager.createResource<Sampler>(samplerInfo);
        }

        // Normals sampler
        {
            VkSamplerCreateInfo samplerInfo = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
            samplerInfo.magFilter = VK_FILTER_NEAREST;
            samplerInfo.minFilter = VK_FILTER_NEAREST;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.anisotropyEnable = VK_FALSE;
            samplerInfo.maxAnisotropy = 1.0f;
            samplerInfo.compareEnable = VK_FALSE;
            samplerInfo.minLod = 0.0f;
            samplerInfo.maxLod = 0.0f;

            normalsSampler = resourceManager.createResource<Sampler>(samplerInfo);
        }
    }

    // Spatial Filtering
    {
        DescriptorLayoutBuilder layoutBuilder{4};
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // raw ao
        layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // edge data
        layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // filtered ao
        layoutBuilder.addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // debug image
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = layoutBuilder.build(
            VK_SHADER_STAGE_COMPUTE_BIT,
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );

        spatialFilteringSetLayout = resourceManager.createResource<DescriptorSetLayout>(layoutCreateInfo);

        VkPushConstantRange pushConstants{};
        pushConstants.offset = 0;
        pushConstants.size = sizeof(GTAOPushConstants);
        pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        std::array setLayouts{
            resourceManager.getSceneDataLayout(),
            spatialFilteringSetLayout->layout,
        };


        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = nullptr;
        layoutInfo.pSetLayouts = setLayouts.data();
        layoutInfo.setLayoutCount = setLayouts.size();
        layoutInfo.pPushConstantRanges = &pushConstants;
        layoutInfo.pushConstantRangeCount = 1;

        spatialFilteringPipelineLayout = resourceManager.createResource<PipelineLayout>(layoutInfo);
        createSpatialFilteringPipeline();

        spatialFilteringDescriptorBuffer = resourceManager.createResource<DescriptorBufferSampler>(spatialFilteringSetLayout->layout, 1);

        VkImageUsageFlags usage{};
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        VkImageCreateInfo imgInfo = vk_helpers::imageCreateInfo(ambientOcclusionFormat, usage, {RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1});
        denoisedFinalAO = resourceManager.createResource<Image>(imgInfo);
    }
}

GroundTruthAmbientOcclusionPipeline::~GroundTruthAmbientOcclusionPipeline()
{
    // Debug Resources
    resourceManager.destroyResource(std::move(debugImage));

    // Depth Prefilter Resources
    resourceManager.destroyResource(std::move(depthPrefilterSetLayout));
    resourceManager.destroyResource(std::move(depthPrefilterPipelineLayout));
    resourceManager.destroyResource(std::move(depthPrefilterPipeline));

    for (int32_t i = 0; i < DEPTH_PREFILTER_MIP_COUNT; ++i) {
        resourceManager.destroyResource(std::move(depthPrefilterImageViews[i]));
    }

    resourceManager.destroyResource(std::move(depthPrefilterImage));
    resourceManager.destroyResource(std::move(depthSampler));

    resourceManager.destroyResource(std::move(depthPrefilterDescriptorBuffer));

    // AO Resources
    resourceManager.destroyResource(std::move(ambientOcclusionSetLayout));
    resourceManager.destroyResource(std::move(ambientOcclusionPipelineLayout));
    resourceManager.destroyResource(std::move(ambientOcclusionPipeline));

    resourceManager.destroyResource(std::move(depthPrefilterSampler));
    resourceManager.destroyResource(std::move(normalsSampler));
    resourceManager.destroyResource(std::move(ambientOcclusionImage));
    resourceManager.destroyResource(std::move(edgeDataImage));

    resourceManager.destroyResource(std::move(ambientOcclusionDescriptorBuffer));

    // Spatial Filtering Resources
    resourceManager.destroyResource(std::move(spatialFilteringSetLayout));
    resourceManager.destroyResource(std::move(spatialFilteringPipelineLayout));
    resourceManager.destroyResource(std::move(spatialFilteringPipeline));

    resourceManager.destroyResource(std::move(denoisedFinalAO));

    resourceManager.destroyResource(std::move(spatialFilteringDescriptorBuffer));
}

void GroundTruthAmbientOcclusionPipeline::setupDepthPrefilterDescriptorBuffer(const VkImageView& depthImageView)
{
    std::vector<DescriptorImageData> imageDescriptors{};
    imageDescriptors.reserve(1 + DEPTH_PREFILTER_MIP_COUNT + 1);

    imageDescriptors.push_back(
        {
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            {depthSampler->sampler, depthImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            false
        }
    );

    for (int32_t i = 0; i < DEPTH_PREFILTER_MIP_COUNT; ++i) {
        DescriptorImageData imageData{};
        imageData.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        imageData.imageInfo = {VK_NULL_HANDLE, depthPrefilterImageViews[i]->imageView, VK_IMAGE_LAYOUT_GENERAL};
        imageData.bIsPadding = false;

        imageDescriptors.push_back(imageData);
    }

    imageDescriptors.push_back({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        {VK_NULL_HANDLE, debugImage->imageView, VK_IMAGE_LAYOUT_GENERAL},
        false
    });

    depthPrefilterDescriptorBuffer->setupData(imageDescriptors, 0);
}

void GroundTruthAmbientOcclusionPipeline::setupAmbientOcclusionDescriptorBuffer(const VkImageView& normalsImageView)
{
    std::vector<DescriptorImageData> imageDescriptors{};
    imageDescriptors.reserve(4);

    imageDescriptors.push_back(
        {
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            {depthPrefilterSampler->sampler, depthPrefilterImage->imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            false
        });
    imageDescriptors.push_back(
        {
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            {normalsSampler->sampler, normalsImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            false
        });
    imageDescriptors.push_back(
        {
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            {VK_NULL_HANDLE, ambientOcclusionImage->imageView, VK_IMAGE_LAYOUT_GENERAL},
            false
        });
    imageDescriptors.push_back(
        {
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            {VK_NULL_HANDLE, edgeDataImage->imageView, VK_IMAGE_LAYOUT_GENERAL},
            false
        });
    imageDescriptors.push_back({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        {VK_NULL_HANDLE, debugImage->imageView, VK_IMAGE_LAYOUT_GENERAL},
        false
    });

    ambientOcclusionDescriptorBuffer->setupData(imageDescriptors, 0);
}

void GroundTruthAmbientOcclusionPipeline::setupSpatialFilteringDescriptorBuffer()
{
    std::vector<DescriptorImageData> imageDescriptors{};
    imageDescriptors.reserve(5);

    imageDescriptors.push_back(
        {
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            {depthSampler->sampler, ambientOcclusionImage->imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            false
        });
    imageDescriptors.push_back(
        {
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            {depthSampler->sampler, edgeDataImage->imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            false
        });
    imageDescriptors.push_back({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        {VK_NULL_HANDLE, denoisedFinalAO->imageView, VK_IMAGE_LAYOUT_GENERAL},
        false
    });
    imageDescriptors.push_back({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        {VK_NULL_HANDLE, debugImage->imageView, VK_IMAGE_LAYOUT_GENERAL},
        false
    });

    spatialFilteringDescriptorBuffer->setupData(imageDescriptors, 0);
}

void GroundTruthAmbientOcclusionPipeline::draw(VkCommandBuffer cmd, const GTAODrawInfo& drawInfo)
{
    VkDebugUtilsLabelEXT label{};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "GT Ambient Occlusion";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    glm::mat4 projMatrix = drawInfo.camera->getProjMatrix();
    drawInfo.push.depthLinearizeMult = -projMatrix[3][2];
    drawInfo.push.depthLinearizeAdd = projMatrix[2][2];
    if (drawInfo.push.depthLinearizeMult * drawInfo.push.depthLinearizeAdd < 0) {
        drawInfo.push.depthLinearizeAdd = -drawInfo.push.depthLinearizeAdd;
    }

    float tanHalfFOVY = 1.0f / projMatrix[1][1];
    float tanHalfFOVX = 1.0F / projMatrix[0][0];
    drawInfo.push.cameraTanHalfFOV = {tanHalfFOVX, tanHalfFOVY};
    drawInfo.push.ndcToViewMul = {drawInfo.push.cameraTanHalfFOV.x * 2.0f, drawInfo.push.cameraTanHalfFOV.y * -2.0f};
    drawInfo.push.ndcToViewAdd = {drawInfo.push.cameraTanHalfFOV.x * -1.0f, drawInfo.push.cameraTanHalfFOV.y * 1.0f};
    constexpr glm::vec2 texelSize = {1.0f / RENDER_EXTENT_WIDTH, 1.0f / RENDER_EXTENT_HEIGHT};
    drawInfo.push.ndcToViewMul_x_PixelSize = {drawInfo.push.ndcToViewMul.x * texelSize.x, drawInfo.push.ndcToViewMul.y * texelSize.y};

    drawInfo.push.noiseIndex = GTAO_DENOISE_PASSES > 0 ? drawInfo.currentFrame % 64 : 0;

    vk_helpers::imageBarrier(cmd, debugImage->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

    vk_helpers::clearColorImage(cmd, VK_IMAGE_ASPECT_COLOR_BIT, depthPrefilterImage->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    if (!drawInfo.bEnabled) {
        vk_helpers::clearColorImage(cmd, VK_IMAGE_ASPECT_COLOR_BIT, denoisedFinalAO->image, VK_IMAGE_LAYOUT_UNDEFINED,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, {1.0f, 1.0f, 1.0f, 1.0f});
        vk_helpers::imageBarrier(cmd, debugImage->image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                 VK_IMAGE_ASPECT_COLOR_BIT);
        return;
    }
    // Depth Prefilter
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, depthPrefilterPipeline->pipeline);
        vkCmdPushConstants(cmd, depthPrefilterPipelineLayout->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GTAOPushConstants), &drawInfo.push);

        VkDescriptorBufferBindingInfoEXT bindingInfos[2] = {};
        bindingInfos[0] = drawInfo.sceneDataBinding;
        bindingInfos[1] = depthPrefilterDescriptorBuffer->getBindingInfo();
        vkCmdBindDescriptorBuffersEXT(cmd, 2, bindingInfos);

        constexpr std::array<uint32_t, 2> indices{0, 1};
        const std::array offsets{drawInfo.sceneDataOffset, ZERO_DEVICE_SIZE};

        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, depthPrefilterPipelineLayout->layout, 0, 2, indices.data(),
                                           offsets.data());

        // shader only operates on 8,8 work groups, mip 0 will operate on 2x2 texels, so its 16x16 as expected
        const auto x = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_WIDTH / 16.0f));
        const auto y = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_HEIGHT / 16.0f));
        vkCmdDispatch(cmd, x, y, 1);
    }

    vk_helpers::imageBarrier(cmd, depthPrefilterImage->image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::imageBarrier(cmd, ambientOcclusionImage->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
    // Ambient Occlusion
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ambientOcclusionPipeline->pipeline);
        vkCmdPushConstants(cmd, ambientOcclusionPipelineLayout->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GTAOPushConstants), &drawInfo.push);

        VkDescriptorBufferBindingInfoEXT bindingInfos[2] = {};
        bindingInfos[0] = drawInfo.sceneDataBinding;
        bindingInfos[1] = ambientOcclusionDescriptorBuffer->getBindingInfo();
        vkCmdBindDescriptorBuffersEXT(cmd, 2, bindingInfos);

        constexpr std::array<uint32_t, 2> indices{0, 1};
        const std::array offsets{drawInfo.sceneDataOffset, ZERO_DEVICE_SIZE};

        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, depthPrefilterPipelineLayout->layout, 0, 2, indices.data(),
                                           offsets.data());

        const auto x = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_WIDTH / 16.0f));
        const auto y = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_HEIGHT / 16.0f));
        vkCmdDispatch(cmd, x, y, 1);
    }


    vk_helpers::imageBarrier(cmd, ambientOcclusionImage->image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::imageBarrier(cmd, denoisedFinalAO->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                             VK_IMAGE_ASPECT_COLOR_BIT);
    // Spatial Filtering
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, spatialFilteringPipeline->pipeline);
        vkCmdPushConstants(cmd, spatialFilteringPipelineLayout->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GTAOPushConstants), &drawInfo.push);

        VkDescriptorBufferBindingInfoEXT bindingInfos[2] = {};
        bindingInfos[0] = drawInfo.sceneDataBinding;
        bindingInfos[1] = spatialFilteringDescriptorBuffer->getBindingInfo();
        vkCmdBindDescriptorBuffersEXT(cmd, 2, bindingInfos);

        constexpr std::array<uint32_t, 2> indices{0, 1};
        const std::array offsets{drawInfo.sceneDataOffset, ZERO_DEVICE_SIZE};

        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, spatialFilteringPipelineLayout->layout, 0, 2, indices.data(),
                                           offsets.data());

        // each dispatch operates on 2x1 pixels
        const auto x = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_WIDTH / (16.0f * 2.0f)));
        const auto y = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_HEIGHT / 16.0f));
        vkCmdDispatch(cmd, x, y, 1);
    }

    vk_helpers::imageBarrier(cmd, denoisedFinalAO->image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             VK_IMAGE_ASPECT_COLOR_BIT);


    vk_helpers::imageBarrier(cmd, debugImage->image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             VK_IMAGE_ASPECT_COLOR_BIT);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void GroundTruthAmbientOcclusionPipeline::reloadShaders()
{
    createDepthPrefilterPipeline();
    createAmbientOcclusionPipeline();
    createSpatialFilteringPipeline();
}

void GroundTruthAmbientOcclusionPipeline::createDepthPrefilterPipeline()
{
    resourceManager.destroyResource(std::move(depthPrefilterPipeline));
    ShaderModulePtr shader = resourceManager.createResource<ShaderModule>("shaders/ambient_occlusion/ground_truth/gtao_depth_prefilter.comp");

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = shader->shader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.layout = depthPrefilterPipelineLayout->layout;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    depthPrefilterPipeline = resourceManager.createResource<Pipeline>(pipelineInfo);
}

void GroundTruthAmbientOcclusionPipeline::createAmbientOcclusionPipeline()
{
    resourceManager.destroyResource(std::move(ambientOcclusionPipeline));
    ShaderModulePtr shader = resourceManager.createResource<ShaderModule>("shaders/ambient_occlusion/ground_truth/gtao_main_pass.comp");

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = shader->shader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.layout = ambientOcclusionPipelineLayout->layout;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    ambientOcclusionPipeline = resourceManager.createResource<Pipeline>(pipelineInfo);
}

void GroundTruthAmbientOcclusionPipeline::createSpatialFilteringPipeline()
{
    resourceManager.destroyResource(std::move(spatialFilteringPipeline));
    ShaderModulePtr shader = resourceManager.createResource<ShaderModule>("shaders/ambient_occlusion/ground_truth/gtao_spatial_filter.comp");

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = shader->shader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.layout = spatialFilteringPipelineLayout->layout;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    spatialFilteringPipeline = resourceManager.createResource<Pipeline>(pipelineInfo);
}
}
