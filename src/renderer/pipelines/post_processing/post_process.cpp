//
// Created by William on 2024-12-07.
//

#include "post_process.h"

#include "src/renderer/vk_descriptors.h"


PostProcessPipeline::PostProcessPipeline(VulkanContext& context)
    : context(context)
{}

PostProcessPipeline::~PostProcessPipeline()
{
    cleanup();
}

void PostProcessPipeline::init()
{
    createDescriptorLayout();
    createPipelineLayout();
    createPipeline();

    descriptorBuffer = DescriptorBufferSampler(context.instance, context.device, context.physicalDevice, context.allocator, descriptorSetLayout, 1);
}

void PostProcessPipeline::createDescriptorLayout()
{
    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // taa resolve image
    layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // post process result

    descriptorSetLayout = layoutBuilder.build(context.device, VK_SHADER_STAGE_COMPUTE_BIT, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
}

void PostProcessPipeline::createPipelineLayout()
{
    VkPushConstantRange pushConstants{};
    pushConstants.offset = 0;
    pushConstants.size = sizeof(PostProcessData);
    pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayout setLayouts[1];
    setLayouts[0] = descriptorSetLayout;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.pSetLayouts = setLayouts;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstants;
    layoutInfo.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(context.device, &layoutInfo, nullptr, &pipelineLayout));
}

void PostProcessPipeline::createPipeline()
{
    VkShaderModule computeShader;
    if (!vk_helpers::loadShaderModule("shaders/postProcess.comp.spv", context.device, &computeShader)) {
        throw std::runtime_error("Error when building compute shader (postProcess.comp.spv)");
    }

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

    VK_CHECK(vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));
    vkDestroyShaderModule(context.device, computeShader, nullptr);
}

void PostProcessPipeline::setupDescriptorBuffer(const PostProcessDescriptorBufferInfo& bufferInfo)
{
    std::vector<DescriptorImageData> descriptors;
    descriptors.reserve(2);

    VkDescriptorImageInfo inputImage{};
    inputImage.sampler = bufferInfo.nearestSampler;
    inputImage.imageView = bufferInfo.inputImage;
    inputImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo outputImage{};
    outputImage.imageView = bufferInfo.outputImage;
    outputImage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    descriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, inputImage, false});
    descriptors.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, outputImage, false});

    descriptorBuffer.setupData(context.device, descriptors, 0);
}

void PostProcessPipeline::draw(VkCommandBuffer cmd, const PostProcessDrawInfo& drawInfo) const
{
    if (!descriptorBuffer.isIndexOccupied(0)) {
        fmt::print("Descriptor buffer not yet set up");
        return;
    }

    VkDebugUtilsLabelEXT label{};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Post Process Pass";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

    PostProcessData data{};
    data.width = static_cast<int32_t>(drawInfo.renderExtent.width);
    data.height = static_cast<int32_t>(drawInfo.renderExtent.height);
    data.postProcessFlags = static_cast<uint32_t>(drawInfo.postProcessFlags);
    data.padding = 0;

    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PostProcessData), &data);

    const VkDescriptorBufferBindingInfoEXT bindingInfo = descriptorBuffer.getDescriptorBufferBindingInfo();
    vkCmdBindDescriptorBuffersEXT(cmd, 1, &bindingInfo);

    constexpr VkDeviceSize zeroOffset{0};
    constexpr uint32_t descriptorIndex{0};
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorIndex, &zeroOffset);

    const auto x = static_cast<uint32_t>(std::ceil(static_cast<float>(drawInfo.renderExtent.width) / 16.0f));
    const auto y = static_cast<uint32_t>(std::ceil(static_cast<float>(drawInfo.renderExtent.height) / 16.0f));
    vkCmdDispatch(cmd, x, y, 1);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void PostProcessPipeline::cleanup()
{
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

    descriptorBuffer.destroy(context.device, context.allocator);
}
