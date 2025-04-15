//
// Created by William on 2025-03-23.
//

#include "ground_truth_ambient_occlusion_pipeline.h"

#include <volk/volk.h>

#include "src/renderer/renderer_constants.h"
#include "src/renderer/vk_descriptors.h"
#include "ambient_occlusion_types.h"

will_engine::ambient_occlusion::GroundTruthAmbientOcclusionPipeline::GroundTruthAmbientOcclusionPipeline(
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
        debugImage = resourceManager.createImage(imgInfo);
    }

    // Depth Pre-filtering
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // MRT depth buffer
        layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ao depth mip 0
        layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ao depth mip 1
        layoutBuilder.addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ao depth mip 2
        layoutBuilder.addBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ao depth mip 3
        layoutBuilder.addBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ao depth mip 4
        layoutBuilder.addBinding(6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // debug image

        depthPrefilterSetLayout = resourceManager.createDescriptorSetLayout(layoutBuilder, VK_SHADER_STAGE_COMPUTE_BIT,
                                                                            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

        VkPushConstantRange pushConstants{};
        pushConstants.offset = 0;
        pushConstants.size = sizeof(GTAOPushConstants);
        pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayout setLayouts[2];
        setLayouts[0] = resourceManager.getSceneDataLayout();
        setLayouts[1] = depthPrefilterSetLayout;

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = nullptr;
        layoutInfo.pSetLayouts = setLayouts;
        layoutInfo.setLayoutCount = 2;
        layoutInfo.pPushConstantRanges = &pushConstants;
        layoutInfo.pushConstantRangeCount = 1;

        depthPrefilterPipelineLayout = resourceManager.createPipelineLayout(layoutInfo);
        createDepthPrefilterPipeline();

        depthPrefilterDescriptorBuffer = resourceManager.createDescriptorBufferSampler(depthPrefilterSetLayout, 1);

        VkImageUsageFlags usage{};
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        VkImageCreateInfo imgInfo = vk_helpers::imageCreateInfo(depthPrefilterFormat, usage, {RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1});
        // 5 mips, suggested by Intel's implementation
        // https://github.com/GameTechDev/XeGTAO
        imgInfo.mipLevels = DEPTH_PREFILTER_MIP_COUNT;
        depthPrefilterImage = resourceManager.createImage(imgInfo);
        VkImageViewCreateInfo viewInfo = vk_helpers::imageviewCreateInfo(depthPrefilterFormat, depthPrefilterImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

        for (int32_t i = 0; i < DEPTH_PREFILTER_MIP_COUNT; ++i) {
            viewInfo.subresourceRange.baseMipLevel = i;
            depthPrefilterImageViews[i] = resourceManager.createImageView(viewInfo);
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

        depthSampler = resourceManager.createSampler(samplerInfo);
    }

    // AO Calculation
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // pre-filtered depth
        layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // MRT normal buffer
        layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ao output
        layoutBuilder.addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // edge data output
        layoutBuilder.addBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // debug image

        ambientOcclusionSetLayout = resourceManager.createDescriptorSetLayout(layoutBuilder, VK_SHADER_STAGE_COMPUTE_BIT,
                                                                              VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

        VkPushConstantRange pushConstants{};
        pushConstants.offset = 0;
        pushConstants.size = sizeof(GTAOPushConstants);
        pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayout setLayouts[2];
        setLayouts[0] = resourceManager.getSceneDataLayout();
        setLayouts[1] = ambientOcclusionSetLayout;

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = nullptr;
        layoutInfo.pSetLayouts = setLayouts;
        layoutInfo.setLayoutCount = 2;
        layoutInfo.pPushConstantRanges = &pushConstants;
        layoutInfo.pushConstantRangeCount = 1;

        ambientOcclusionPipelineLayout = resourceManager.createPipelineLayout(layoutInfo);
        createAmbientOcclusionPipeline();

        ambientOcclusionDescriptorBuffer = resourceManager.createDescriptorBufferSampler(ambientOcclusionSetLayout, 1);

        VkImageUsageFlags usage{};
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

        VkImageCreateInfo imgInfo = vk_helpers::imageCreateInfo(ambientOcclusionFormat, usage, {RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1});
        ambientOcclusionImage = resourceManager.createImage(imgInfo);

        usage = {};
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

        imgInfo = vk_helpers::imageCreateInfo(edgeDataFormat, usage, {RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1});
        edgeDataImage = resourceManager.createImage(imgInfo);


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

            depthPrefilterSampler = resourceManager.createSampler(samplerInfo);
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

            normalsSampler = resourceManager.createSampler(samplerInfo);
        }
    }

    // Spatial Filtering
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // raw ao
        layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // edge data
        layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // filtered ao
        layoutBuilder.addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // debug image

        spatialFilteringSetLayout = resourceManager.createDescriptorSetLayout(layoutBuilder, VK_SHADER_STAGE_COMPUTE_BIT,
                                                                              VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

        VkPushConstantRange pushConstants{};
        pushConstants.offset = 0;
        pushConstants.size = sizeof(GTAOPushConstants);
        pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayout setLayouts[2];
        setLayouts[0] = resourceManager.getSceneDataLayout();
        setLayouts[1] = spatialFilteringSetLayout;

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = nullptr;
        layoutInfo.pSetLayouts = setLayouts;
        layoutInfo.setLayoutCount = 2;
        layoutInfo.pPushConstantRanges = &pushConstants;
        layoutInfo.pushConstantRangeCount = 1;

        spatialFilteringPipelineLayout = resourceManager.createPipelineLayout(layoutInfo);
        createSpatialFilteringPipeline();

        spatialFilteringDescriptorBuffer = resourceManager.createDescriptorBufferSampler(spatialFilteringSetLayout, 1);

        VkImageUsageFlags usage{};
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

        VkImageCreateInfo imgInfo = vk_helpers::imageCreateInfo(ambientOcclusionFormat, usage, {RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1});
        denoisedFinalAO = resourceManager.createImage(imgInfo);
    }
}

will_engine::ambient_occlusion::GroundTruthAmbientOcclusionPipeline::~GroundTruthAmbientOcclusionPipeline()
{
    // Debug Resources
    resourceManager.destroyImage(debugImage);

    // Depth Prefilter Resources
    resourceManager.destroyDescriptorSetLayout(depthPrefilterSetLayout);
    resourceManager.destroyPipelineLayout(depthPrefilterPipelineLayout);
    resourceManager.destroyPipeline(depthPrefilterPipeline);

    for (int32_t i = 0; i < DEPTH_PREFILTER_MIP_COUNT; ++i) {
        resourceManager.destroyImageView(depthPrefilterImageViews[i]);
    }

    resourceManager.destroyImage(depthPrefilterImage);
    resourceManager.destroySampler(depthSampler);

    resourceManager.destroyDescriptorBuffer(depthPrefilterDescriptorBuffer);

    // AO Resources
    resourceManager.destroyDescriptorSetLayout(ambientOcclusionSetLayout);
    resourceManager.destroyPipelineLayout(ambientOcclusionPipelineLayout);
    resourceManager.destroyPipeline(ambientOcclusionPipeline);

    resourceManager.destroySampler(depthPrefilterSampler);
    resourceManager.destroySampler(normalsSampler);
    resourceManager.destroyImage(ambientOcclusionImage);
    resourceManager.destroyImage(edgeDataImage);

    resourceManager.destroyDescriptorBuffer(ambientOcclusionDescriptorBuffer);

    // Spatial Filtering Resources
    resourceManager.destroyDescriptorSetLayout(spatialFilteringSetLayout);
    resourceManager.destroyPipelineLayout(spatialFilteringPipelineLayout);
    resourceManager.destroyPipeline(spatialFilteringPipeline);

    resourceManager.destroyImage(denoisedFinalAO);

    resourceManager.destroyDescriptorBuffer(spatialFilteringDescriptorBuffer);
}

void will_engine::ambient_occlusion::GroundTruthAmbientOcclusionPipeline::setupDepthPrefilterDescriptorBuffer(const VkImageView& depthImageView)
{
    std::vector<DescriptorImageData> imageDescriptors{};
    imageDescriptors.reserve(1 + DEPTH_PREFILTER_MIP_COUNT + 1);

    imageDescriptors.push_back(
        {
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            {depthSampler, depthImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            false
        }
    );

    for (int32_t i = 0; i < DEPTH_PREFILTER_MIP_COUNT; ++i) {
        DescriptorImageData imageData{};
        imageData.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        imageData.imageInfo = {VK_NULL_HANDLE, depthPrefilterImageViews[i], VK_IMAGE_LAYOUT_GENERAL};
        imageData.bIsPadding = false;

        imageDescriptors.push_back(imageData);
    }

    imageDescriptors.push_back({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        {VK_NULL_HANDLE, debugImage.imageView, VK_IMAGE_LAYOUT_GENERAL},
        false
    });

    resourceManager.setupDescriptorBufferSampler(depthPrefilterDescriptorBuffer, imageDescriptors, 0);
}

void will_engine::ambient_occlusion::GroundTruthAmbientOcclusionPipeline::setupAmbientOcclusionDescriptorBuffer(const VkImageView& normalsImageView)
{
    std::vector<DescriptorImageData> imageDescriptors{};
    imageDescriptors.reserve(4);

    imageDescriptors.push_back(
        {
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            {depthPrefilterSampler, depthPrefilterImage.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            false
        });
    imageDescriptors.push_back(
        {
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            {normalsSampler, normalsImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            false
        });
    imageDescriptors.push_back(
        {
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            {VK_NULL_HANDLE, ambientOcclusionImage.imageView, VK_IMAGE_LAYOUT_GENERAL},
            false
        });
    imageDescriptors.push_back(
        {
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            {VK_NULL_HANDLE, edgeDataImage.imageView, VK_IMAGE_LAYOUT_GENERAL},
            false
        });
    imageDescriptors.push_back({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        {VK_NULL_HANDLE, debugImage.imageView, VK_IMAGE_LAYOUT_GENERAL},
        false
    });

    resourceManager.setupDescriptorBufferSampler(ambientOcclusionDescriptorBuffer, imageDescriptors, 0);
}

void will_engine::ambient_occlusion::GroundTruthAmbientOcclusionPipeline::setupSpatialFilteringDescriptorBuffer()
{
    std::vector<DescriptorImageData> imageDescriptors{};
    imageDescriptors.reserve(5);

    imageDescriptors.push_back(
        {
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            {depthSampler, ambientOcclusionImage.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            false
        });
    imageDescriptors.push_back(
        {
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            {depthSampler, edgeDataImage.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            false
        });
    imageDescriptors.push_back({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        {VK_NULL_HANDLE, denoisedFinalAO.imageView, VK_IMAGE_LAYOUT_GENERAL},
        false
    });
    imageDescriptors.push_back({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        {VK_NULL_HANDLE, debugImage.imageView, VK_IMAGE_LAYOUT_GENERAL},
        false
    });

    resourceManager.setupDescriptorBufferSampler(spatialFilteringDescriptorBuffer, imageDescriptors, 0);
}

void will_engine::ambient_occlusion::GroundTruthAmbientOcclusionPipeline::draw(VkCommandBuffer cmd, const GTAODrawInfo& drawInfo)
{
    VkDebugUtilsLabelEXT label{};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "GT Ambient Occlusion";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    gtaoPush.debug = drawInfo.pushConstants.debug;

    glm::mat4 projMatrix = drawInfo.camera->getProjMatrix();
    gtaoPush.depthLinearizeMult = -projMatrix[3][2];
    gtaoPush.depthLinearizeAdd = projMatrix[2][2];
    if (gtaoPush.depthLinearizeMult * gtaoPush.depthLinearizeAdd < 0) {
        gtaoPush.depthLinearizeAdd = -gtaoPush.depthLinearizeAdd;
    }

    float tanHalfFOVY = 1.0f / projMatrix[1][1];
    float tanHalfFOVX = 1.0F / projMatrix[0][0];
    gtaoPush.cameraTanHalfFOV = {tanHalfFOVX, tanHalfFOVY};
    gtaoPush.ndcToViewMul = {gtaoPush.cameraTanHalfFOV.x * 2.0f, gtaoPush.cameraTanHalfFOV.y * -2.0f};
    gtaoPush.ndcToViewAdd = {gtaoPush.cameraTanHalfFOV.x * -1.0f, gtaoPush.cameraTanHalfFOV.y * 1.0f};
    constexpr glm::vec2 texelSize = {1.0f / RENDER_EXTENT_WIDTH, 1.0f / RENDER_EXTENT_HEIGHT};
    gtaoPush.ndcToViewMul_x_PixelSize = {gtaoPush.ndcToViewMul.x * texelSize.x, gtaoPush.ndcToViewMul.y * texelSize.y};


    gtaoPush.noiseIndex = GTAO_DENOISE_PASSES > 0 ? drawInfo.currentFrame % 64 : 0;

    vk_helpers::transitionImage(cmd, debugImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

    vk_helpers::clearColorImage(cmd, VK_IMAGE_ASPECT_COLOR_BIT, depthPrefilterImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    // Depth Prefilter
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, depthPrefilterPipeline);
        vkCmdPushConstants(cmd, depthPrefilterPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GTAOPushConstants), &gtaoPush);

        VkDescriptorBufferBindingInfoEXT bindingInfos[2] = {};
        bindingInfos[0] = drawInfo.sceneDataBinding;
        bindingInfos[1] = depthPrefilterDescriptorBuffer.getDescriptorBufferBindingInfo();
        vkCmdBindDescriptorBuffersEXT(cmd, 2, bindingInfos);

        constexpr std::array<uint32_t, 2> indices{0, 1};
        const std::array offsets{drawInfo.sceneDataOffset, ZERO_DEVICE_SIZE};

        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, depthPrefilterPipelineLayout, 0, 2, indices.data(), offsets.data());

        // shader only operates on 8,8 work groups, mip 0 will operate on 2x2 texels, so its 16x16 as expected
        const auto x = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_WIDTH / 16.0f));
        const auto y = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_HEIGHT / 16.0f));
        vkCmdDispatch(cmd, x, y, 1);
    }

    vk_helpers::transitionImage(cmd, depthPrefilterImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, ambientOcclusionImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
    // Ambient Occlusion
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ambientOcclusionPipeline);
        vkCmdPushConstants(cmd, ambientOcclusionPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GTAOPushConstants), &gtaoPush);

        VkDescriptorBufferBindingInfoEXT bindingInfos[2] = {};
        bindingInfos[0] = drawInfo.sceneDataBinding;
        bindingInfos[1] = ambientOcclusionDescriptorBuffer.getDescriptorBufferBindingInfo();
        vkCmdBindDescriptorBuffersEXT(cmd, 2, bindingInfos);

        constexpr std::array<uint32_t, 2> indices{0, 1};
        const std::array offsets{drawInfo.sceneDataOffset, ZERO_DEVICE_SIZE};

        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, depthPrefilterPipelineLayout, 0, 2, indices.data(), offsets.data());

        const auto x = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_WIDTH / 16.0f));
        const auto y = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_HEIGHT / 16.0f));
        vkCmdDispatch(cmd, x, y, 1);
    }


    vk_helpers::transitionImage(cmd, ambientOcclusionImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, denoisedFinalAO.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                VK_IMAGE_ASPECT_COLOR_BIT);
    // Spatial Filtering
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, spatialFilteringPipeline);
        vkCmdPushConstants(cmd, spatialFilteringPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GTAOPushConstants), &gtaoPush);

        VkDescriptorBufferBindingInfoEXT bindingInfos[2] = {};
        bindingInfos[0] = drawInfo.sceneDataBinding;
        bindingInfos[1] = spatialFilteringDescriptorBuffer.getDescriptorBufferBindingInfo();
        vkCmdBindDescriptorBuffersEXT(cmd, 2, bindingInfos);

        constexpr std::array<uint32_t, 2> indices{0, 1};
        const std::array offsets{drawInfo.sceneDataOffset, ZERO_DEVICE_SIZE};

        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, spatialFilteringPipelineLayout, 0, 2, indices.data(), offsets.data());

        // each dispatch operates on 2x1 pixels
        const auto x = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_WIDTH / (16.0f * 2.0f)));
        const auto y = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_HEIGHT / 16.0f));
        vkCmdDispatch(cmd, x, y, 1);
    }

    vk_helpers::transitionImage(cmd, denoisedFinalAO.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_IMAGE_ASPECT_COLOR_BIT);


    vk_helpers::transitionImage(cmd, debugImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_IMAGE_ASPECT_COLOR_BIT);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void will_engine::ambient_occlusion::GroundTruthAmbientOcclusionPipeline::reloadShaders()
{
    createDepthPrefilterPipeline();
    createAmbientOcclusionPipeline();
    createSpatialFilteringPipeline();
}

void will_engine::ambient_occlusion::GroundTruthAmbientOcclusionPipeline::createDepthPrefilterPipeline()
{
    resourceManager.destroyPipeline(depthPrefilterPipeline);
    VkShaderModule computeShader = resourceManager.createShaderModule("shaders/ambient_occlusion/ground_truth/gtao_depth_prefilter.comp");

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = computeShader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.layout = depthPrefilterPipelineLayout;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    depthPrefilterPipeline = resourceManager.createComputePipeline(pipelineInfo);
    resourceManager.destroyShaderModule(computeShader);
}

void will_engine::ambient_occlusion::GroundTruthAmbientOcclusionPipeline::createAmbientOcclusionPipeline()
{
    resourceManager.destroyPipeline(ambientOcclusionPipeline);
    VkShaderModule computeShader = resourceManager.createShaderModule("shaders/ambient_occlusion/ground_truth/gtao_main_pass.comp");

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = computeShader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.layout = ambientOcclusionPipelineLayout;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    ambientOcclusionPipeline = resourceManager.createComputePipeline(pipelineInfo);
    resourceManager.destroyShaderModule(computeShader);
}

void will_engine::ambient_occlusion::GroundTruthAmbientOcclusionPipeline::createSpatialFilteringPipeline()
{
    resourceManager.destroyPipeline(spatialFilteringPipeline);
    VkShaderModule computeShader = resourceManager.createShaderModule("shaders/ambient_occlusion/ground_truth/gtao_spatial_filter.comp");

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = computeShader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.layout = spatialFilteringPipelineLayout;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    spatialFilteringPipeline = resourceManager.createComputePipeline(pipelineInfo);
    resourceManager.destroyShaderModule(computeShader);
}
