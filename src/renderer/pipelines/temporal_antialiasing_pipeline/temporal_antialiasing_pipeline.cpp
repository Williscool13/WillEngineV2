//
// Created by William on 2025-01-25.
//

#include "temporal_antialiasing_pipeline.h"

#include "temporal_antialiasing_pipeline_types.h"
#include "src/renderer/renderer_constants.h"
#include "src/renderer/resource_manager.h"
#include "src/renderer/vk_descriptors.h"


will_engine::temporal_antialiasing_pipeline::TemporalAntialiasingPipeline::TemporalAntialiasingPipeline(ResourceManager& resourceManager)
    : resourceManager(resourceManager)
{
    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // drawImage
    layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // history
    layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // depth
    layoutBuilder.addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // velocity
    layoutBuilder.addBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // output

    descriptorSetLayout = resourceManager.createDescriptorSetLayout(layoutBuilder, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    VkPushConstantRange pushConstants{};
    pushConstants.offset = 0;
    pushConstants.size = sizeof(TemporalAntialiasingPushConstants);
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

    descriptorBuffer = resourceManager.createDescriptorBufferSampler(descriptorSetLayout, 1);
}

will_engine::temporal_antialiasing_pipeline::TemporalAntialiasingPipeline::~TemporalAntialiasingPipeline()
{
    resourceManager.destroyPipeline(pipeline);
    resourceManager.destroyPipelineLayout(pipelineLayout);
    resourceManager.destroyDescriptorSetLayout(descriptorSetLayout);
    resourceManager.destroyDescriptorBuffer(descriptorBuffer);
}

void will_engine::temporal_antialiasing_pipeline::TemporalAntialiasingPipeline::setupDescriptorBuffer(const TemporalAntialiasingDescriptor& descriptor)
{
    std::vector<DescriptorImageData> descriptors;
    descriptors.reserve(5);

    VkDescriptorImageInfo drawImage{};
    drawImage.sampler = descriptor.sampler;
    drawImage.imageView = descriptor.drawImage;
    drawImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo history{};
    history.sampler = descriptor.sampler;
    history.imageView = descriptor.historyBuffer;
    history.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo depth{};
    depth.sampler = descriptor.sampler;
    depth.imageView = descriptor.depthBuffer;
    depth.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo velocity{};
    velocity.sampler = descriptor.sampler;
    velocity.imageView = descriptor.velocityBuffer;
    velocity.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo output{};
    output.imageView = descriptor.outputTarget;
    output.imageLayout = VK_IMAGE_LAYOUT_GENERAL;


    descriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, drawImage, false});
    descriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, history, false});
    descriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, depth, false});
    descriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, velocity, false});
    descriptors.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, output, false});

    resourceManager.setupDescriptorBufferSampler(descriptorBuffer, descriptors, 0);
}

void will_engine::temporal_antialiasing_pipeline::TemporalAntialiasingPipeline::draw(VkCommandBuffer cmd, const TemporalAntialiasingDrawInfo& drawInfo) const
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

    TemporalAntialiasingPushConstants properties{};;
    properties.blendValue = drawInfo.blendValue;
    properties.taaDebug = drawInfo.debugMode;

    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(TemporalAntialiasingPushConstants), &properties);

    VkDescriptorBufferBindingInfoEXT bindingInfos[2] = {};
    bindingInfos[0] = drawInfo.sceneDataBinding;
    bindingInfos[1] = descriptorBuffer.getDescriptorBufferBindingInfo();
    vkCmdBindDescriptorBuffersEXT(cmd, 2, bindingInfos);


    constexpr VkDeviceSize zeroOffset{0};
    constexpr uint32_t sceneDataIndex{0};
    constexpr uint32_t descriptorIndex{1};

    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &sceneDataIndex, &drawInfo.sceneDataOffset);
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 1, 1, &descriptorIndex, &zeroOffset);

    const auto x = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_WIDTH / 16.0f));
    const auto y = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_HEIGHT / 16.0f));
    vkCmdDispatch(cmd, x, y, 1);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void will_engine::temporal_antialiasing_pipeline::TemporalAntialiasingPipeline::createPipeline()
{
    resourceManager.destroyPipeline(pipeline);
    VkShaderModule computeShader = resourceManager.createShaderModule("shaders/taa.comp");

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
