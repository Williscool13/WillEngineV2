//
// Created by William on 2024-12-07.
//

#include "frustum_culling_pipeline.h"

FrustumCullingPipeline::FrustumCullingPipeline(VulkanContext& context)
    : context(context)
{}

FrustumCullingPipeline::~FrustumCullingPipeline()
{
    cleanup();
}

void FrustumCullingPipeline::init(const FrustumCullPipelineCreateInfo& createInfo)
{
    sceneDataLayout = createInfo.sceneDataLayout;
    frustumCullLayout = createInfo.frustumCullLayout;

    createPipelineLayout();
    createPipeline();
}

void FrustumCullingPipeline::createPipelineLayout()
{
    VkDescriptorSetLayout layouts[2];
    layouts[0] = sceneDataLayout;
    layouts[1] = frustumCullLayout;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 2;
    layoutInfo.pSetLayouts = layouts;
    layoutInfo.pPushConstantRanges = nullptr;
    layoutInfo.pushConstantRangeCount = 0;

    VK_CHECK(vkCreatePipelineLayout(context.device, &layoutInfo, nullptr, &pipelineLayout));
}

void FrustumCullingPipeline::createPipeline()
{
    VkShaderModule computeShader;
    if (!vk_helpers::loadShaderModule("shaders/frustumCull.comp.spv", context.device, &computeShader)) {
        throw std::runtime_error("Error when building compute shader (frustumCull.comp.spv)");
    }

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

    VK_CHECK(vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));
    vkDestroyShaderModule(context.device, computeShader, nullptr);
}

void FrustumCullingPipeline::draw(VkCommandBuffer cmd, const FrustumCullDrawInfo& drawInfo) const
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Frustum Culling";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

    constexpr uint32_t sceneDataIndex{0};
    constexpr uint32_t addressesIndex{1};

    for (RenderObject* renderObject : drawInfo.renderObjects) {
        VkDescriptorBufferBindingInfoEXT bindingInfo[2];
        bindingInfo[0] = drawInfo.sceneData.getDescriptorBufferBindingInfo();
        bindingInfo[1] = renderObject->getFrustumCullingAddressesDescriptorBuffer().getDescriptorBufferBindingInfo();

        VkDeviceSize sceneDataOffset = drawInfo.currentFrameOverlap * drawInfo.sceneData.getDescriptorBufferSize();
        VkDeviceSize addressesOffset = drawInfo.currentFrameOverlap * renderObject->getFrustumCullingAddressesDescriptorBuffer().getDescriptorBufferSize();

        vkCmdBindDescriptorBuffersEXT(cmd, 2, bindingInfo);
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &sceneDataIndex, &sceneDataOffset);
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 1, 1, &addressesIndex, &addressesOffset);

        vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(static_cast<float>(renderObject->getDrawIndirectCommandCount()) / 64.0f)), 1, 1);
    }

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void FrustumCullingPipeline::cleanup()
{
    if (pipeline) {
        vkDestroyPipeline(context.device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
    if (pipelineLayout) {
        vkDestroyPipelineLayout(context.device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }
}
