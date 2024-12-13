//
// Created by William on 2024-12-07.
//

#include "deferred_resolve.h"

#include "src/renderer/vk_descriptors.h"

DeferredResolvePipeline::DeferredResolvePipeline(VulkanContext& context)
    : context(context)
{}

DeferredResolvePipeline::~DeferredResolvePipeline()
{
    cleanup();
}

void DeferredResolvePipeline::init(const DeferredResolvePipelineCreateInfo& createInfo)
{
    sceneDataLayout = createInfo.sceneDataLayout;
    emptyLayout = createInfo.emptyLayout;
    environmentMapLayout = createInfo.environmentMapLayout;

    createDescriptorLayouts();
    createPipelineLayout();
    createPipeline();

    resolveDescriptorBuffer = DescriptorBufferSampler(context.instance, context.device, context.physicalDevice, context.allocator, resolveTargetLayout, 1);
}

void DeferredResolvePipeline::createDescriptorLayouts()
{
    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    layoutBuilder.addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    layoutBuilder.addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    layoutBuilder.addBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    resolveTargetLayout = layoutBuilder.build(context.device, VK_SHADER_STAGE_COMPUTE_BIT, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
}

void DeferredResolvePipeline::createPipelineLayout()
{
    VkPushConstantRange pushConstants = {};
    pushConstants.offset = 0;
    pushConstants.size = sizeof(DeferredResolveData);
    pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayout setLayouts[4];
    setLayouts[0] = sceneDataLayout;
    setLayouts[1] = resolveTargetLayout;
    setLayouts[2] = emptyLayout; // todo: lights
    setLayouts[3] = environmentMapLayout;


    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.pSetLayouts = setLayouts;
    layoutInfo.setLayoutCount = 4;
    layoutInfo.pPushConstantRanges = &pushConstants;
    layoutInfo.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(context.device, &layoutInfo, nullptr, &pipelineLayout));
}

void DeferredResolvePipeline::createPipeline()
{
    VkShaderModule computeShader;
    if (!vk_helpers::loadShaderModule("shaders/deferredResolve.comp.spv", context.device, &computeShader)) {
        throw std::runtime_error("Error when building compute shader (deferredResolve.comp.spv)");
    }

    VkPipelineShaderStageCreateInfo stageInfo = {};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = computeShader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VK_CHECK(vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));
    vkDestroyShaderModule(context.device, computeShader, nullptr);
}

void DeferredResolvePipeline::setupDescriptorBuffer(const DeferredResolveDescriptorBufferInfo& drawInfo)
{
    std::vector<DescriptorImageData> renderTargetDescriptors;
    renderTargetDescriptors.reserve(6);

    VkDescriptorImageInfo normalTarget = {};
    normalTarget.sampler = drawInfo.nearestSampler;
    normalTarget.imageView = drawInfo.normalTarget;
    normalTarget.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo albedoTarget = {};
    albedoTarget.sampler = drawInfo.nearestSampler;
    albedoTarget.imageView = drawInfo.albedoTarget;
    albedoTarget.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo pbrDataTarget = {};
    pbrDataTarget.sampler = drawInfo.nearestSampler;
    pbrDataTarget.imageView = drawInfo.pbrTarget;
    pbrDataTarget.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo depthImageTarget = {};
    depthImageTarget.sampler = drawInfo.nearestSampler;
    depthImageTarget.imageView = drawInfo.depthTarget;
    depthImageTarget.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo velocityTarget = {};
    velocityTarget.sampler = drawInfo.nearestSampler;
    velocityTarget.imageView = drawInfo.velocityTarget;
    velocityTarget.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo drawImageTarget = {};
    drawImageTarget.imageView = drawInfo.outputTarget;
    drawImageTarget.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, normalTarget, false});
    renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, albedoTarget, false});
    renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, pbrDataTarget, false});
    renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, depthImageTarget, false});
    renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, velocityTarget, false});
    renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, drawImageTarget, false});

    resolveDescriptorBuffer.setupData(context.device, renderTargetDescriptors, 0);
}

void DeferredResolvePipeline::draw(VkCommandBuffer cmd, const DeferredResolveDrawInfo& drawInfo) const
{
    if (!resolveDescriptorBuffer.isIndexOccupied(0)) {
        fmt::print("Descriptor buffer not yet set up");
        return;
    }

    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Deferred Resolve Pass";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

    DeferredResolveData pushConstants = {};
    pushConstants.width = drawInfo.renderExtent.width;
    pushConstants.height = drawInfo.renderExtent.height;
    pushConstants.debug = drawInfo.debugMode;

    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(DeferredResolveData), &pushConstants);

    VkDescriptorBufferBindingInfoEXT bindingInfos[3] = {};
    bindingInfos[0] = drawInfo.sceneData.getDescriptorBufferBindingInfo();
    bindingInfos[1] = resolveDescriptorBuffer.getDescriptorBufferBindingInfo();
    bindingInfos[2] = drawInfo.environment->getDiffSpecMapDescriptorBuffer().getDescriptorBufferBindingInfo();
    vkCmdBindDescriptorBuffersEXT(cmd, 3, bindingInfos);

    constexpr VkDeviceSize zeroOffset{0};
    constexpr uint32_t sceneDataIndex{0};
    constexpr uint32_t renderTargetsIndex{1};
    constexpr uint32_t environmentIndex{2};
    const VkDeviceSize environmentMapOffset = drawInfo.environment->getDiffSpecMapOffset(drawInfo.environmentMapIndex);

    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &sceneDataIndex, &zeroOffset);
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 1, 1, &renderTargetsIndex, &zeroOffset);
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 3, 1, &environmentIndex, &environmentMapOffset);

    const auto x = static_cast<uint32_t>(std::ceil(static_cast<float>(drawInfo.renderExtent.width) / 16.0f));
    const auto y = static_cast<uint32_t>(std::ceil(static_cast<float>(drawInfo.renderExtent.height) / 16.0f));
    vkCmdDispatch(cmd, x, y, 1);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void DeferredResolvePipeline::cleanup()
{
    if (pipeline) {
        vkDestroyPipeline(context.device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
    if (pipelineLayout) {
        vkDestroyPipelineLayout(context.device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }
    if (resolveTargetLayout) {
        vkDestroyDescriptorSetLayout(context.device, resolveTargetLayout, nullptr);
        resolveTargetLayout = VK_NULL_HANDLE;
    }

    resolveDescriptorBuffer.destroy(context.device, context.allocator);
}
