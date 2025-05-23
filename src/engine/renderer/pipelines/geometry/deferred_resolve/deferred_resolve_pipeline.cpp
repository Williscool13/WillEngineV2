//
// Created by William on 2025-01-25.
//

#include "deferred_resolve_pipeline.h"

#include <array>

#include "deferred_resolve_pipeline_types.h"
#include "engine/renderer/renderer_constants.h"
#include "engine/renderer/resource_manager.h"


will_engine::deferred_resolve::DeferredResolvePipeline::DeferredResolvePipeline(ResourceManager& resourceManager,
                                                                                VkDescriptorSetLayout environmentIBLLayout,
                                                                                VkDescriptorSetLayout cascadeUniformLayout,
                                                                                VkDescriptorSetLayout cascadeSamplerLayout)
    : resourceManager(resourceManager)
{
    VkPushConstantRange pushConstants = {};
    pushConstants.offset = 0;
    pushConstants.size = sizeof(DeferredResolvePushConstants);
    pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayout setLayouts[5];
    setLayouts[0] = resourceManager.getSceneDataLayout();
    setLayouts[1] = resourceManager.getRenderTargetsLayout();
    setLayouts[2] = environmentIBLLayout;
    setLayouts[3] = cascadeUniformLayout;
    setLayouts[4] = cascadeSamplerLayout;

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.pSetLayouts = setLayouts;
    layoutInfo.setLayoutCount = 5;
    layoutInfo.pPushConstantRanges = &pushConstants;
    layoutInfo.pushConstantRangeCount = 1;

    pipelineLayout = resourceManager.createPipelineLayout(layoutInfo);

    createPipeline();

    resolveDescriptorBuffer = resourceManager.createDescriptorBufferSampler(resourceManager.getRenderTargetsLayout(), 1);
}

will_engine::deferred_resolve::DeferredResolvePipeline::~DeferredResolvePipeline()
{
    resourceManager.destroy(pipelineLayout);
    resourceManager.destroy(pipeline);
    resourceManager.destroy(resolveDescriptorBuffer);
}

void will_engine::deferred_resolve::DeferredResolvePipeline::setupDescriptorBuffer(const DeferredResolveDescriptor& drawInfo)
{
    std::vector<DescriptorImageData> renderTargetDescriptors;
    renderTargetDescriptors.reserve(8);

    VkDescriptorImageInfo normalTarget = {};
    normalTarget.sampler = drawInfo.sampler;
    normalTarget.imageView = drawInfo.normalTarget;
    normalTarget.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo albedoTarget = {};
    albedoTarget.sampler = drawInfo.sampler;
    albedoTarget.imageView = drawInfo.albedoTarget;
    albedoTarget.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo pbrDataTarget = {};
    pbrDataTarget.sampler = drawInfo.sampler;
    pbrDataTarget.imageView = drawInfo.pbrTarget;
    pbrDataTarget.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo depthImageTarget = {};
    depthImageTarget.sampler = drawInfo.sampler;
    depthImageTarget.imageView = drawInfo.depthTarget;
    depthImageTarget.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo velocityTarget = {};
    velocityTarget.sampler = drawInfo.sampler;
    velocityTarget.imageView = drawInfo.velocityTarget;
    velocityTarget.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo aoTarget = {};
    aoTarget.sampler = drawInfo.sampler;
    aoTarget.imageView = drawInfo.aoTarget;
    aoTarget.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo contactShadow = {};
    contactShadow.sampler = drawInfo.sampler;
    contactShadow.imageView = drawInfo.contactShadowsTarget;
    contactShadow.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo drawImageTarget = {};
    drawImageTarget.imageView = drawInfo.outputTarget;
    drawImageTarget.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, normalTarget, false});
    renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, albedoTarget, false});
    renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, pbrDataTarget, false});
    renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, depthImageTarget, false});
    renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, velocityTarget, false});
    renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, aoTarget, false});
    renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, contactShadow, false});
    renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, drawImageTarget, false});

    resourceManager.setupDescriptorBufferSampler(resolveDescriptorBuffer, renderTargetDescriptors, 0);
}

void will_engine::deferred_resolve::DeferredResolvePipeline::draw(VkCommandBuffer cmd, const DeferredResolveDrawInfo& drawInfo) const
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

    DeferredResolvePushConstants pushConstants = {};
    pushConstants.width = RENDER_EXTENT_WIDTH;
    pushConstants.height = RENDER_EXTENT_HEIGHT;
    pushConstants.deferredDebug = drawInfo.deferredDebug;
    pushConstants.disableShadows = drawInfo.bEnableShadowMap ? 0 : 1;
    pushConstants.disableContactShadows = drawInfo.bEnableContactShadows ? 0 : 1;
    pushConstants.pcfLevel = drawInfo.csmPcf;
    pushConstants.nearPlane = drawInfo.nearPlane;
    pushConstants.farPlane = drawInfo.farPlane;

    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(DeferredResolvePushConstants), &pushConstants);

    VkDescriptorBufferBindingInfoEXT bindingInfos[5] = {};
    bindingInfos[0] = drawInfo.sceneDataBinding;
    bindingInfos[1] = resolveDescriptorBuffer.getDescriptorBufferBindingInfo();
    bindingInfos[2] = drawInfo.environmentIBLBinding;
    bindingInfos[3] = drawInfo.cascadeUniformBinding;
    bindingInfos[4] = drawInfo.cascadeSamplerBinding;
    vkCmdBindDescriptorBuffersEXT(cmd, 5, bindingInfos);

    constexpr std::array<uint32_t, 5> indices{0, 1, 2, 3, 4};
    const std::array offsets{
        drawInfo.sceneDataOffset,
        ZERO_DEVICE_SIZE,
        drawInfo.environmentIBLOffset,
        drawInfo.cascadeUniformOffset,
        ZERO_DEVICE_SIZE
    };


    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 5, indices.data(), offsets.data());

    const auto x = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_WIDTH / 16.0f));
    const auto y = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_HEIGHT / 16.0f));
    vkCmdDispatch(cmd, x, y, 1);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void will_engine::deferred_resolve::DeferredResolvePipeline::createPipeline()
{
    resourceManager.destroy(pipeline);
    VkShaderModule deferredResolveShader = resourceManager.createShaderModule("shaders/deferredResolve.comp");

    VkPipelineShaderStageCreateInfo stageInfo = {};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = deferredResolveShader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    pipeline = resourceManager.createComputePipeline(pipelineInfo);
    resourceManager.destroy(deferredResolveShader);
}
