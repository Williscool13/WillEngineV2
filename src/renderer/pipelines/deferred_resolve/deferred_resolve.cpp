//
// Created by William on 2025-01-25.
//

#include "deferred_resolve.h"

#include "volk.h"
#include "src/renderer/renderer_constants.h"
#include "src/renderer/resource_manager.h"
#include "src/renderer/vk_descriptors.h"

will_engine::deferred_resolve::DeferredResolvePipeline::DeferredResolvePipeline(ResourceManager& resourceManager, VkDescriptorSetLayout environmentIBLLayout, VkDescriptorSetLayout cascadeUniformLayout, VkDescriptorSetLayout cascadeSamplerlayout) : resourceManager(resourceManager)
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
    setLayouts[4] = cascadeSamplerlayout;

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.pSetLayouts = setLayouts;
    layoutInfo.setLayoutCount = 5;
    layoutInfo.pPushConstantRanges = &pushConstants;
    layoutInfo.pushConstantRangeCount = 1;

    pipelineLayout = resourceManager.createPipelineLayout(layoutInfo);

    VkShaderModule deferredResolveShader = resourceManager.createShaderModule("shaders/deferredResolve.comp.spv");

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
    resourceManager.destroyShaderModule(deferredResolveShader);

    resolveDescriptorBuffer = resourceManager.createDescriptorBufferSampler(resourceManager.getRenderTargetsLayout(), 1);
}

will_engine::deferred_resolve::DeferredResolvePipeline::~DeferredResolvePipeline()
{
    resourceManager.destroyPipelineLayout(pipelineLayout);
    resourceManager.destroyPipeline(pipeline);
    resourceManager.destroyDescriptorBuffer(resolveDescriptorBuffer);
}

void will_engine::deferred_resolve::DeferredResolvePipeline::setupDescriptorBuffer(const DeferredResolveDescriptor& drawInfo)
{
    std::vector<DescriptorImageData> renderTargetDescriptors;
    renderTargetDescriptors.reserve(6);

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

    VkDescriptorImageInfo drawImageTarget = {};
    drawImageTarget.imageView = drawInfo.outputTarget;
    drawImageTarget.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, normalTarget, false});
    renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, albedoTarget, false});
    renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, pbrDataTarget, false});
    renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, depthImageTarget, false});
    renderTargetDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, velocityTarget, false});
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
    pushConstants.debug = 0;
    pushConstants.disableShadows = 0;
    pushConstants.pcfLevel = 0;

    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(DeferredResolvePushConstants), &pushConstants);

    VkDescriptorBufferBindingInfoEXT bindingInfos[5] = {};
    bindingInfos[0] = drawInfo.sceneDataBinding;
    bindingInfos[1] = resolveDescriptorBuffer.getDescriptorBufferBindingInfo();
    bindingInfos[2] = drawInfo.environmentIBLBinding;
    bindingInfos[3] = drawInfo.cascadeUniformBinding;
    bindingInfos[4] = drawInfo.cascadeSamplerBinding;
    vkCmdBindDescriptorBuffersEXT(cmd, 5, bindingInfos);

    constexpr VkDeviceSize zeroOffset{0};
    constexpr uint32_t sceneDataIndex{0};
    constexpr uint32_t renderTargetsIndex{1};
    constexpr uint32_t environmentIndex{2};
    constexpr uint32_t cascadedShadowMapUniformIndex{3};
    constexpr uint32_t cascadedShadowMapSamplerIndex{4};

    const VkDeviceSize sceneDataOffset{drawInfo.sceneDataOffset};
    const VkDeviceSize environmentOffset{drawInfo.environmentIBLOffset};
    const VkDeviceSize cascadedShadowMapUniformOffset{drawInfo.cascadeUniformOffset};
    constexpr VkDeviceSize cascadedShadowMapSamplerOffset{0};

    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &sceneDataIndex, &sceneDataOffset);
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 1, 1, &renderTargetsIndex, &zeroOffset);
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 2, 1, &environmentIndex, &environmentOffset);
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 3, 1, &cascadedShadowMapUniformIndex, &cascadedShadowMapUniformOffset);
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 4, 1, &cascadedShadowMapSamplerIndex, &cascadedShadowMapSamplerOffset);

    const auto x = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_WIDTH / 16.0f));
    const auto y = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_HEIGHT / 16.0f));
    vkCmdDispatch(cmd, x, y, 1);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}
