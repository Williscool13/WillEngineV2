//
// Created by William on 2025-04-14.
//

#include "contact_shadows_pipeline.h"

#include "contact_shadows_pipeline_types.h"
#include "src/renderer/resource_manager.h"

namespace will_engine::contact_shadows_pipeline
{
ContactShadowsPipeline::ContactShadowsPipeline(ResourceManager& resourceManager) : resourceManager(resourceManager)
{
    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // MRT depth buffer
    layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ss contact shadows image
    layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // debug image

    descriptorSetLayout = resourceManager.createDescriptorSetLayout(layoutBuilder, VK_SHADER_STAGE_COMPUTE_BIT,
                                                                        VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    VkPushConstantRange pushConstants{};
    pushConstants.offset = 0;
    pushConstants.size = sizeof(ContactShadowsPushConstants);
    pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayout setLayouts[2];
    setLayouts[0] = resourceManager.getSceneDataLayout();
    setLayouts[1] = descriptorSetLayout;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.pSetLayouts = setLayouts;
    layoutInfo.setLayoutCount = 2;
    layoutInfo.pPushConstantRanges = &pushConstants;
    layoutInfo.pushConstantRangeCount = 1;

    pipelineLayout = resourceManager.createPipelineLayout(layoutInfo);
    createPipeline();

    VkSamplerCreateInfo samplerInfo = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    depthSampler = resourceManager.createSampler(samplerInfo);

    VkImageUsageFlags usage{};
    usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VkImageCreateInfo imgInfo = vk_helpers::imageCreateInfo(contactShadowFormat, usage, {RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1});
    contactShadowImage = resourceManager.createImage(imgInfo);

    usage = {};
    usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    imgInfo = vk_helpers::imageCreateInfo(debugFormat, usage, {RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1});
    debugImage = resourceManager.createImage(imgInfo);

    descriptorBufferSampler = resourceManager.createDescriptorBufferSampler(descriptorSetLayout, 1);
}

ContactShadowsPipeline::~ContactShadowsPipeline()
{
    resourceManager.destroyPipeline(pipeline);
    resourceManager.destroyPipelineLayout(pipelineLayout);
    resourceManager.destroyDescriptorSetLayout(descriptorSetLayout);

    resourceManager.destroySampler(depthSampler);

    resourceManager.destroyImage(contactShadowImage);
    resourceManager.destroyImage(debugImage);
    resourceManager.destroyDescriptorBuffer(descriptorBufferSampler);
}

void ContactShadowsPipeline::setupDescriptorBuffer(const VkImageView& depthImageView)
{
    std::vector<DescriptorImageData> imageDescriptors{};
    imageDescriptors.reserve(3);

    imageDescriptors.push_back(
        {
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            {depthSampler, depthImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            false
        });
    imageDescriptors.push_back(
        {
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            {VK_NULL_HANDLE, contactShadowImage.imageView, VK_IMAGE_LAYOUT_GENERAL},
            false
        });
    imageDescriptors.push_back({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        {VK_NULL_HANDLE, debugImage.imageView, VK_IMAGE_LAYOUT_GENERAL},
        false
    });

    resourceManager.setupDescriptorBufferSampler(descriptorBufferSampler, imageDescriptors, 0);
}

void ContactShadowsPipeline::draw(VkCommandBuffer cmd, const ContactShadowsDrawInfo& drawInfo)
{
    vk_helpers::transitionImage(cmd, debugImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::clearColorImage(cmd, VK_IMAGE_ASPECT_COLOR_BIT, contactShadowImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    ContactShadowsPushConstants push{};

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ContactShadowsPushConstants), &push);

    VkDescriptorBufferBindingInfoEXT bindingInfos[2] = {};
    bindingInfos[0] = drawInfo.sceneDataBinding;
    bindingInfos[1] = descriptorBufferSampler.getDescriptorBufferBindingInfo();
    vkCmdBindDescriptorBuffersEXT(cmd, 2, bindingInfos);

    constexpr std::array<uint32_t, 2> indices{0, 1};
    const std::array offsets{drawInfo.sceneDataOffset, ZERO_DEVICE_SIZE};

    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 2, indices.data(), offsets.data());

    auto x = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_WIDTH / 16.0f));
    auto y = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_HEIGHT / 16.0f));
    vkCmdDispatch(cmd, x, y, 1);
}

void ContactShadowsPipeline::createPipeline()
{
    resourceManager.destroyPipeline(pipeline);
    VkShaderModule computeShader = resourceManager.createShaderModule("shaders/shadows/contact_shadow_pass.comp");

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = computeShader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    pipeline = resourceManager.createComputePipeline(pipelineInfo);
    resourceManager.destroyShaderModule(computeShader);

}
}
