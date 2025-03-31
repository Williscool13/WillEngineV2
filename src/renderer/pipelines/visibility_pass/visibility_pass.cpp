//
// Created by William on 2025-01-26.
//

#include "visibility_pass.h"

#include <ranges>

#include "volk/volk.h"

#include "src/renderer/assets/render_object/render_object.h"

will_engine::visibility_pass::VisibilityPassPipeline::VisibilityPassPipeline(ResourceManager& resourceManager)
    : resourceManager(resourceManager)
{
    VkDescriptorSetLayout layouts[2];
    layouts[0] = resourceManager.getSceneDataLayout();
    layouts[1] = resourceManager.getFrustumCullLayout();

    VkPushConstantRange pushConstantRange;
    pushConstantRange.size = sizeof(VisibilityPassPushConstants);
    pushConstantRange.offset = 0;
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 2;
    layoutInfo.pSetLayouts = layouts;
    layoutInfo.pPushConstantRanges = &pushConstantRange;
    layoutInfo.pushConstantRangeCount = 1;
    pipelineLayout = resourceManager.createPipelineLayout(layoutInfo);


    createPipeline();
}

will_engine::visibility_pass::VisibilityPassPipeline::~VisibilityPassPipeline()
{
    resourceManager.destroyPipeline(pipeline);
    resourceManager.destroyPipelineLayout(pipelineLayout);
}

void will_engine::visibility_pass::VisibilityPassPipeline::draw(VkCommandBuffer cmd, const VisibilityPassDrawInfo& drawInfo) const
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Frustum Culling";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);


    VisibilityPassPushConstants pushConstants = {};
    pushConstants.enable = drawInfo.bEnableFrustumCulling;
    pushConstants.shadowPass = drawInfo.bIsShadowPass;

    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(VisibilityPassPushConstants), &pushConstants);


    constexpr uint32_t sceneDataIndex{0};
    constexpr uint32_t addressesIndex{1};

    for (RenderObject* renderObject : drawInfo.renderObjects) {
        if (!renderObject->canDraw()) { continue; }

        VkDescriptorBufferBindingInfoEXT bindingInfo[2];
        bindingInfo[0] = drawInfo.sceneDataBinding;
        bindingInfo[1] = renderObject->getFrustumCullingAddressesDescriptorBuffer().getDescriptorBufferBindingInfo();

        VkDeviceSize sceneDataOffset = drawInfo.sceneDataOffset;
        VkDeviceSize addressesOffset = renderObject->getFrustumCullingAddressesDescriptorBuffer().getDescriptorBufferSize() * drawInfo.currentFrameOverlap;

        vkCmdBindDescriptorBuffersEXT(cmd, 2, bindingInfo);
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &sceneDataIndex, &sceneDataOffset);
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 1, 1, &addressesIndex, &addressesOffset);

        vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(static_cast<float>(renderObject->getDrawIndirectCommandCount()) / 64.0f)), 1, 1);

        vk_helpers::synchronizeUniform(cmd, renderObject->getIndirectBuffer(drawInfo.currentFrameOverlap), VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
    }

    vkCmdEndDebugUtilsLabelEXT(cmd);
}


void will_engine::visibility_pass::VisibilityPassPipeline::createPipeline()
{
    resourceManager.destroyPipeline(pipeline);
    VkShaderModule computeShader = resourceManager.createShaderModule("shaders/visibility_pass.comp");

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = computeShader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo;
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    pipeline = resourceManager.createComputePipeline(pipelineInfo);
    resourceManager.destroyShaderModule(computeShader);
}
