//
// Created by William on 2025-03-23.
//

#include "ground_truth_ambient_occlusion.h"

#include "src/renderer/renderer_constants.h"
#include "src/renderer/vk_descriptors.h"
#include "src/renderer/lighting/ambient_occlusion/ambient_occlusion_types.h"

will_engine::ambient_occlusion::GroundTruthAmbientOcclusionPipeline::GroundTruthAmbientOcclusionPipeline(ResourceManager& resourceManager) : resourceManager(resourceManager)
{
    // Depth Pre-filtering
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // depth image
        layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ao depth mip 0
        layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ao depth mip 1
        layoutBuilder.addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ao depth mip 2
        layoutBuilder.addBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ao depth mip 3
        layoutBuilder.addBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ao depth mip 4

        depthPrefilterSetLayout = resourceManager.createDescriptorSetLayout(layoutBuilder, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

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
    }
}

will_engine::ambient_occlusion::GroundTruthAmbientOcclusionPipeline::~GroundTruthAmbientOcclusionPipeline()
{}

void will_engine::ambient_occlusion::GroundTruthAmbientOcclusionPipeline::setupDepthPrefilterDescriptorBuffer(VkSampler depthImageSampler, VkImageView depthImageView)
{
    std::vector<DescriptorImageData> imageDescriptors{};
    imageDescriptors.reserve(1 + DEPTH_PREFILTER_MIP_COUNT);

    imageDescriptors.push_back(
        {
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            {depthImageSampler, depthImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
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

    resourceManager.setupDescriptorBufferSampler(depthPrefilterDescriptorBuffer, imageDescriptors, 0);
}

void will_engine::ambient_occlusion::GroundTruthAmbientOcclusionPipeline::createDepthPrefilterPipeline()
{
    resourceManager.destroyPipeline(depthPrefilterPipeline);
    VkShaderModule computeShader = resourceManager.createShaderModule("shaders/gtaodepthprefilter.comp");

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
