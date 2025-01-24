//
// Created by William on 2025-01-22.
//

#include "basic_compute_pipeline.h"

#include <volk.h>

#include "src/renderer/renderer_constants.h"
#include "src/renderer/vk_descriptors.h"
#include "src/renderer/vk_helpers.h"
#include "src/renderer/descriptor_buffer/descriptor_buffer_sampler.h"
#include "src/renderer/descriptor_buffer/descriptor_buffer_types.h"

namespace will_engine::basic_compute
{
BasicComputePipeline::BasicComputePipeline(VulkanContext& context) : context(context)

{
    DescriptorLayoutBuilder builder;
    builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    descriptorSetLayout = builder.build(context.device, VK_SHADER_STAGE_COMPUTE_BIT, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    VkPipelineLayoutCreateInfo layoutCreateInfo{};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCreateInfo.pNext = nullptr;
    layoutCreateInfo.pSetLayouts = &descriptorSetLayout;
    layoutCreateInfo.setLayoutCount = 1;
    layoutCreateInfo.pPushConstantRanges = nullptr;
    layoutCreateInfo.pushConstantRangeCount = 0;

    VK_CHECK(vkCreatePipelineLayout(context.device, &layoutCreateInfo, nullptr, &pipelineLayout));


    VkShaderModule gradientShader;
    if (!vk_helpers::loadShaderModule("shaders/basic/compute.comp.spv", context.device, &gradientShader)) {
        throw std::runtime_error("Error when building the compute shader (compute.comp.spv)");
    }

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.pNext = nullptr;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = gradientShader;
    stageinfo.pName = "main"; // entry point in shader

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = pipelineLayout;
    computePipelineCreateInfo.stage = stageinfo;
    computePipelineCreateInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VK_CHECK(vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &pipeline));

    // Cleanup
    vkDestroyShaderModule(context.device, gradientShader, nullptr);


    samplerDescriptorBuffer = DescriptorBufferSampler(context, descriptorSetLayout, 1);
}

BasicComputePipeline::~BasicComputePipeline()
{
    if (context.device == VK_NULL_HANDLE) { return; }
    if (pipeline) {
        vkDestroyPipeline(context.device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
    if (pipelineLayout) {
        vkDestroyPipelineLayout(context.device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }
    if (descriptorSetLayout) {
        vkDestroyDescriptorSetLayout(context.device, descriptorSetLayout, nullptr);
        descriptorSetLayout = VK_NULL_HANDLE;
    }

    samplerDescriptorBuffer.destroy(context.allocator);
}

void BasicComputePipeline::setupDescriptors(const ComputeDescriptorInfo& descriptorInfo)
{
    std::vector<will_engine::DescriptorImageData> imageDescriptor;
    imageDescriptor.reserve(1);

    VkDescriptorImageInfo drawImageDescriptor{};
    drawImageDescriptor.imageView = descriptorInfo.inputImage;
    drawImageDescriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    imageDescriptor.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, drawImageDescriptor, false});

    samplerDescriptorBuffer.setupData(context.device, imageDescriptor, 0);
}

void BasicComputePipeline::draw(VkCommandBuffer cmd, ComputeDrawInfo drawInfo) const
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

    VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[1]{};
    descriptorBufferBindingInfo[0] = samplerDescriptorBuffer.getDescriptorBufferBindingInfo();
    vkCmdBindDescriptorBuffersEXT(cmd, 1, descriptorBufferBindingInfo);

    constexpr uint32_t bufferIndexImage = 0;
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &bufferIndexImage, &ZERO_DEVICE_SIZE);

    vkCmdDispatch(cmd, std::ceil(drawInfo.renderExtent.width / 16.0), std::ceil(drawInfo.renderExtent.height / 16.0), 1);
}
}
