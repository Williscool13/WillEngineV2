//
// Created by William on 2025-05-02.
//

#include "debug_composite_pipeline.h"

#include "engine/renderer/resource_manager.h"
#include "engine/renderer/vk_descriptors.h"
#include "engine/renderer/resources/pipeline.h"
#include "engine/renderer/resources/pipeline_layout.h"
#include "engine/renderer/resources/shader_module.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_sampler.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_types.h"
#include "volk/volk.h"

namespace will_engine::renderer
{
DebugCompositePipeline::DebugCompositePipeline(ResourceManager& resourceManager) : resourceManager(resourceManager)
{
    DescriptorLayoutBuilder layoutBuilder{2};
    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // Debug output (flipped image!)
    layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // Final Image

    VkDescriptorSetLayoutCreateInfo createInfo = layoutBuilder.build(
        VK_SHADER_STAGE_COMPUTE_BIT,
        VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    descriptorSetLayout = resourceManager.createResource<DescriptorSetLayout>(createInfo);

    const std::array descriptorSetLayouts{resourceManager.getSceneDataLayout(), descriptorSetLayout->layout};

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.pSetLayouts = descriptorSetLayouts.data();
    layoutInfo.setLayoutCount = 2;
    layoutInfo.pPushConstantRanges = nullptr;
    layoutInfo.pushConstantRangeCount = 0;

    pipelineLayout = resourceManager.createResource<PipelineLayout>(layoutInfo);

    createPipeline();

    descriptorBuffer = resourceManager.createResource<DescriptorBufferSampler>(descriptorSetLayout->layout, 1);
}

DebugCompositePipeline::~DebugCompositePipeline()
{
    resourceManager.destroyResource(std::move(pipeline));
    resourceManager.destroyResource(std::move(pipelineLayout));
    resourceManager.destroyResource(std::move(descriptorSetLayout));
    resourceManager.destroyResource(std::move(descriptorBuffer));
}

void DebugCompositePipeline::setupDescriptorBuffer(VkImageView debugTarget, VkImageView finalImageView)
{
    std::vector<DescriptorImageData> descriptors;
    descriptors.reserve(2);

    VkDescriptorImageInfo inputImage{};
    inputImage.sampler = resourceManager.getDefaultSamplerNearest();
    inputImage.imageView = debugTarget;
    inputImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo outputImage{};
    outputImage.imageView = finalImageView;
    outputImage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    descriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, inputImage, false});
    descriptors.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, outputImage, false});

    descriptorBuffer->setupData(descriptors, 0);
}

void DebugCompositePipeline::draw(VkCommandBuffer cmd, const DebugCompositePipelineDrawInfo& drawInfo) const
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline);

    const std::array bindingInfos{
        drawInfo.sceneDataBinding,
        descriptorBuffer->getBindingInfo()
    };
    vkCmdBindDescriptorBuffersEXT(cmd, 2, bindingInfos.data());

    constexpr std::array indices{0u, 1u};
    const std::array<VkDeviceSize, 2> offsets{drawInfo.sceneDataOffset, 0};
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout->layout, 0, 2, indices.data(), offsets.data());

    const auto x = static_cast<uint32_t>(std::ceil(drawInfo.extents.width / 16.0f));
    const auto y = static_cast<uint32_t>(std::ceil(drawInfo.extents.height / 16.0f));
    vkCmdDispatch(cmd, x, y, 1);
}

void DebugCompositePipeline::createPipeline()
{
    resourceManager.destroyResource(std::move(pipeline));
    ShaderModulePtr shader = resourceManager.createResource<ShaderModule>("shaders/debug/debug_composite.comp");

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = shader->shader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.layout = pipelineLayout->layout;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    pipeline = resourceManager.createResource<Pipeline>(pipelineInfo);
}
}
