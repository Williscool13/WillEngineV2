//
// Created by William on 2024-12-07.
//

#include "temporal_antialiasing.h"

#include "src/renderer/vk_descriptors.h"

TaaPipeline::TaaPipeline(VulkanContext& context)
    : context(context)
{}

TaaPipeline::~TaaPipeline()
{
    cleanup();
}

void TaaPipeline::init()
{
    createDescriptorLayout();
    createPipelineLayout();
    createPipeline();

    descriptorBuffer = DescriptorBufferSampler(context.instance, context.device, context.physicalDevice, context.allocator, descriptorSetLayout, 1);
}

void TaaPipeline::createDescriptorLayout()
{
    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // drawImage
    layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // history
    layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // depth
    layoutBuilder.addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // velocity
    layoutBuilder.addBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // output

    descriptorSetLayout = layoutBuilder.build(context.device, VK_SHADER_STAGE_COMPUTE_BIT, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
}

void TaaPipeline::createPipelineLayout()
{
    VkPushConstantRange pushConstants{};
    pushConstants.offset = 0;
    pushConstants.size = sizeof(TaaProperties);
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

void TaaPipeline::createPipeline()
{
    VkShaderModule computeShader;
    if (!vk_helpers::loadShaderModule("shaders/taa.comp.spv", context.device, &computeShader)) {
        throw std::runtime_error("Error when building compute shader (taa.comp.spv)");
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

void TaaPipeline::setupDescriptorBuffer(const TaaDescriptorBufferInfo& bufferInfo)
{
    std::vector<DescriptorImageData> descriptors;
    descriptors.reserve(5);

    VkDescriptorImageInfo drawImage{};
    drawImage.sampler = bufferInfo.nearestSampler;
    drawImage.imageView = bufferInfo.drawImage;
    drawImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo history{};
    history.sampler = bufferInfo.nearestSampler;
    history.imageView = bufferInfo.historyBuffer;
    history.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo depth{};
    depth.sampler = bufferInfo.nearestSampler;
    depth.imageView = bufferInfo.depthBuffer;
    depth.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo velocity{};
    velocity.sampler = bufferInfo.nearestSampler;
    velocity.imageView = bufferInfo.velocityBuffer;
    velocity.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo output{};
    output.imageView = bufferInfo.outputTarget;
    output.imageLayout = VK_IMAGE_LAYOUT_GENERAL;


    descriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, drawImage, false});
    descriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, history, false});
    descriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, depth, false});
    descriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, velocity, false});
    descriptors.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, output, false});

    descriptorBuffer.setupData(context.device, descriptors, 0);
}

void TaaPipeline::draw(VkCommandBuffer cmd, const TaaDrawInfo& drawInfo) const
{
    if (!descriptorBuffer.isIndexOccupied(0)) {
        fmt::print("Descriptor buffer not yet set up");
        return;
    }

    VkDebugUtilsLabelEXT label{};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Compute TAA Pass";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

    TaaProperties properties{};
    properties.width = static_cast<int32_t>(drawInfo.renderExtent.width);
    properties.height = static_cast<int32_t>(drawInfo.renderExtent.height);
    properties.texelSize = {
        1.0f / static_cast<float>(drawInfo.renderExtent.width),
        1.0f / static_cast<float>(drawInfo.renderExtent.height)
    };
    properties.minBlend = drawInfo.minBlend;
    properties.maxBlend = drawInfo.maxBlend;
    properties.velocityWeight = drawInfo.velocityWeight;
    properties.zVelocity = drawInfo.camera.getZVelocity();
    properties.bEnabled = drawInfo.enabled;
    properties.taaDebug = drawInfo.debugMode;

    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(TaaProperties), &properties);

    VkDescriptorBufferBindingInfoEXT bindingInfo = descriptorBuffer.getDescriptorBufferBindingInfo();
    vkCmdBindDescriptorBuffersEXT(cmd, 1, &bindingInfo);

    constexpr VkDeviceSize zeroOffset{0};
    constexpr uint32_t descriptorIndex{0};
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorIndex, &zeroOffset);

    const auto x = static_cast<uint32_t>(std::ceil(static_cast<float>(drawInfo.renderExtent.width) / 16.0f));
    const auto y = static_cast<uint32_t>(std::ceil(static_cast<float>(drawInfo.renderExtent.height) / 16.0f));
    vkCmdDispatch(cmd, x, y, 1);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void TaaPipeline::cleanup()
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
