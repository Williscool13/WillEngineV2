//
// Created by William on 2024-12-07.
//

#include "frustum_cull_pipeline.h"

FrustumCullPipeline::FrustumCullPipeline(VulkanContext& context)
    : context(context)
{}

FrustumCullPipeline::~FrustumCullPipeline()
{
    cleanup();
}

void FrustumCullPipeline::init(const FrustumCullPipelineCreateInfo& createInfo)
{
    sceneDataLayout = createInfo.sceneDataLayout;
    frustumCullLayout = createInfo.frustumCullLayout;

    createPipelineLayout();
    createPipeline();
}

void FrustumCullPipeline::createPipelineLayout()
{
    VkDescriptorSetLayout layouts[2];
    layouts[0] = sceneDataLayout;
    layouts[1] = frustumCullLayout;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 2;
    layoutInfo.pSetLayouts = layouts;

    VK_CHECK(vkCreatePipelineLayout(context.getDevice(), &layoutInfo, nullptr, &pipelineLayout));
}

void FrustumCullPipeline::createPipeline()
{
    VkShaderModule computeShader;
    if (!vk_helpers::loadShaderModule("shaders/frustumCull.comp.spv", context.getDevice(), &computeShader)) {
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

    VK_CHECK(vkCreateComputePipelines(context.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));
    vkDestroyShaderModule(context.getDevice(), computeShader, nullptr);
}

void FrustumCullPipeline::draw(VkCommandBuffer cmd, const FrustumCullDrawInfo& drawInfo) const
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Frustum Culling";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

    constexpr VkDeviceSize offset{0};
    constexpr uint32_t sceneDataIndex{0};
    constexpr uint32_t addressesIndex{1};

    for (RenderObject* renderObject : drawInfo.renderObjects) {
        VkDescriptorBufferBindingInfoEXT bindingInfo[2];
        bindingInfo[0] = drawInfo.sceneData.getDescriptorBufferBindingInfo();
        bindingInfo[1] = renderObject->getFrustumCullingAddressesDescriptorBuffer().getDescriptorBufferBindingInfo();

        vkCmdBindDescriptorBuffersEXT(cmd, 2, bindingInfo);
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &sceneDataIndex, &offset);
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 1, 1, &addressesIndex, &offset);

        vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(static_cast<float>(renderObject->getDrawIndirectCommandCount()) / 64.0f)), 1, 1);
    }

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void FrustumCullPipeline::cleanup()
{
    if (pipeline) {
        vkDestroyPipeline(context.getDevice(), pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
    if (pipelineLayout) {
        vkDestroyPipelineLayout(context.getDevice(), pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }
}
