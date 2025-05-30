//
// Created by William on 2025-01-25.
//

#include "temporal_antialiasing_pipeline.h"

#include "engine/renderer/renderer_constants.h"
#include "engine/renderer/resource_manager.h"
#include "engine/renderer/vk_descriptors.h"

namespace will_engine::renderer
{
TemporalAntialiasingPipeline::TemporalAntialiasingPipeline(ResourceManager& resourceManager)
    : resourceManager(resourceManager)
{
    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // drawImage
    layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // history
    layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // depth
    layoutBuilder.addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // velocity
    layoutBuilder.addBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // output

    descriptorSetLayout = resourceManager.createDescriptorSetLayout(layoutBuilder, VK_SHADER_STAGE_COMPUTE_BIT,
                                                                    VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    VkPushConstantRange pushConstants{};
    pushConstants.offset = 0;
    pushConstants.size = sizeof(TemporalAntialiasingPushConstants);
    pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayout setLayouts[2];
    setLayouts[0] = resourceManager.getSceneDataLayout();
    setLayouts[1] = descriptorSetLayout->layout;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.pSetLayouts = setLayouts;
    layoutInfo.setLayoutCount = 2;
    layoutInfo.pPushConstantRanges = &pushConstants;
    layoutInfo.pushConstantRangeCount = 1;

    pipelineLayout = resourceManager.createResource<PipelineLayout>(layoutInfo);

    createPipeline();

    descriptorBuffer = resourceManager.createResource<DescriptorBufferSampler>(descriptorSetLayout->layout, 1);
}

TemporalAntialiasingPipeline::~TemporalAntialiasingPipeline()
{
    resourceManager.destroyResource(std::move(pipeline));
    resourceManager.destroyResource(std::move(pipelineLayout));
    resourceManager.destroyResource(std::move(descriptorSetLayout));
    resourceManager.destroyResource(std::move(descriptorBuffer));
}

void TemporalAntialiasingPipeline::setupDescriptorBuffer(
    const TemporalAntialiasingDescriptor& descriptor)
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

    descriptorBuffer->setupData(descriptors, 0);
}

void TemporalAntialiasingPipeline::draw(VkCommandBuffer cmd, const TemporalAntialiasingDrawInfo& drawInfo) const
{
    if (!descriptorBuffer) {
        fmt::print("Descriptor buffer not yet set up");
        return;
    }

    VkDebugUtilsLabelEXT label{};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Compute TAA Pass";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline);

    TemporalAntialiasingPushConstants properties{};;
    properties.blendValue = drawInfo.blendValue;
    properties.taaDebug = drawInfo.debugMode;

    vkCmdPushConstants(cmd, pipelineLayout->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(TemporalAntialiasingPushConstants), &properties);

    VkDescriptorBufferBindingInfoEXT bindingInfos[2] = {};
    bindingInfos[0] = drawInfo.sceneDataBinding;
    bindingInfos[1] = descriptorBuffer->getBindingInfo();
    vkCmdBindDescriptorBuffersEXT(cmd, 2, bindingInfos);


    constexpr std::array<uint32_t, 2> indices{0, 1};
    const std::array offsets{drawInfo.sceneDataOffset, ZERO_DEVICE_SIZE};

    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout->layout, 0, 2, indices.data(), offsets.data());

    const auto x = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_WIDTH / 16.0f));
    const auto y = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_HEIGHT / 16.0f));
    vkCmdDispatch(cmd, x, y, 1);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void TemporalAntialiasingPipeline::createPipeline()
{
    resourceManager.destroyResource(std::move(pipeline));
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
    pipelineInfo.layout = pipelineLayout->layout;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    pipeline = resourceManager.createResource<Pipeline>(pipelineInfo);
    resourceManager.destroyShaderModule(computeShader);
}
}
