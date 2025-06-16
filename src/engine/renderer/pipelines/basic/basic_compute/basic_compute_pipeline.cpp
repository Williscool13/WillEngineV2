//
// Created by William on 2025-01-22.
//

#include "basic_compute_pipeline.h"

#include "engine/renderer/renderer_constants.h"
#include "engine/renderer/resource_manager.h"
#include "engine/renderer/vk_descriptors.h"
#include "engine/renderer/resources/pipeline.h"
#include "engine/renderer/resources/pipeline_layout.h"
#include "engine/renderer/resources/shader_module.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_sampler.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_types.h"

namespace will_engine::renderer
{
BasicComputePipeline::BasicComputePipeline(ResourceManager& resourceManager) : resourceManager(resourceManager)

{
    DescriptorLayoutBuilder builder{1};
    builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    VkDescriptorSetLayoutCreateInfo createInfo = builder.build(
        VK_SHADER_STAGE_COMPUTE_BIT,
        VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    descriptorSetLayout = resourceManager.createResource<DescriptorSetLayout>(createInfo);


    VkPipelineLayoutCreateInfo layoutCreateInfo{};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCreateInfo.pNext = nullptr;
    layoutCreateInfo.pSetLayouts = &descriptorSetLayout->layout;
    layoutCreateInfo.setLayoutCount = 1;
    layoutCreateInfo.pPushConstantRanges = nullptr;
    layoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayout = resourceManager.createResource<PipelineLayout>(layoutCreateInfo);

    createPipeline();

    samplerDescriptorBuffer = resourceManager.createResource<DescriptorBufferSampler>(descriptorSetLayout->layout, 1);
}

BasicComputePipeline::~BasicComputePipeline()
{
    resourceManager.destroyResource(std::move(pipeline));
    resourceManager.destroyResource(std::move(pipelineLayout));
    resourceManager.destroyResource(std::move(descriptorSetLayout));
    resourceManager.destroyResource(std::move(samplerDescriptorBuffer));
}

void BasicComputePipeline::setupDescriptors(const ComputeDescriptorInfo& descriptorInfo)
{
    std::vector<DescriptorImageData> imageDescriptor;
    imageDescriptor.reserve(1);

    VkDescriptorImageInfo drawImageDescriptor{};
    drawImageDescriptor.imageView = descriptorInfo.inputImage;
    drawImageDescriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    imageDescriptor.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, drawImageDescriptor, false});

    samplerDescriptorBuffer->setupData(imageDescriptor, 0);
}

void BasicComputePipeline::draw(VkCommandBuffer cmd, const ComputeDrawInfo drawInfo) const
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline);

    VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[1]{};
    descriptorBufferBindingInfo[0] = samplerDescriptorBuffer->getBindingInfo();
    vkCmdBindDescriptorBuffersEXT(cmd, 1, descriptorBufferBindingInfo);

    constexpr uint32_t bufferIndexImage = 0;
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout->layout, 0, 1, &bufferIndexImage, &ZERO_DEVICE_SIZE);

    vkCmdDispatch(cmd, std::ceil(drawInfo.renderExtent.width / 16.0), std::ceil(drawInfo.renderExtent.height / 16.0), 1);
}

void BasicComputePipeline::createPipeline()
{
    resourceManager.destroyResource(std::move(pipeline));
    ShaderModulePtr shader = resourceManager.createResource<ShaderModule>("shaders/basic/compute.comp");

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = shader->shader;
    stageInfo.pName = "main"; // entry point in shader

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = pipelineLayout->layout;
    computePipelineCreateInfo.stage = stageInfo;
    computePipelineCreateInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    pipeline = resourceManager.createResource<Pipeline>(computePipelineCreateInfo);
}
}
