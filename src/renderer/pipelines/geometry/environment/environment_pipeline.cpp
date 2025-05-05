//
// Created by William on 2025-01-25.
//

#include "environment_pipeline.h"

#include "environment_pipeline_types.h"
#include "src/renderer/renderer_constants.h"
#include "src/renderer/resource_manager.h"
#include "src/renderer/vk_helpers.h"
#include "src/renderer/vk_pipelines.h"

will_engine::environment_pipeline::EnvironmentPipeline::EnvironmentPipeline(ResourceManager& resourceManager, VkDescriptorSetLayout environmentMapLayout) : resourceManager(resourceManager)
{
    VkDescriptorSetLayout layouts[2];
    layouts[0] = resourceManager.getSceneDataLayout();
    layouts[1] = environmentMapLayout;

    VkPipelineLayoutCreateInfo layoutInfo = vk_helpers::pipelineLayoutCreateInfo();
    layoutInfo.pNext = nullptr;
    layoutInfo.setLayoutCount = 2;
    layoutInfo.pSetLayouts = layouts;
    layoutInfo.pPushConstantRanges = nullptr;
    layoutInfo.pushConstantRangeCount = 0;

    pipelineLayout = resourceManager.createPipelineLayout(layoutInfo);

    createPipeline();
}

will_engine::environment_pipeline::EnvironmentPipeline::~EnvironmentPipeline()
{
    resourceManager.destroy(pipeline);
    resourceManager.destroy(pipelineLayout);
}

void will_engine::environment_pipeline::EnvironmentPipeline::draw(VkCommandBuffer cmd, const EnvironmentDrawInfo& drawInfo) const
{
    VkDebugUtilsLabelEXT label{};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Environment Map";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    constexpr VkClearValue colorClear = {.color = {0.0f, 0.0f, 0.0f, 0.0f}};
    constexpr VkClearValue depthClear = {.depthStencil = {0.0f, 0u}};

    VkRenderingAttachmentInfo normalAttachment = vk_helpers::attachmentInfo(drawInfo.normalTarget, drawInfo.bClearColor ? &colorClear : nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo albedoAttachment = vk_helpers::attachmentInfo(drawInfo.albedoTarget, drawInfo.bClearColor ? &colorClear : nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo pbrAttachment = vk_helpers::attachmentInfo(drawInfo.pbrTarget, drawInfo.bClearColor ? &colorClear : nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo velocityAttachment = vk_helpers::attachmentInfo(drawInfo.velocityTarget, drawInfo.bClearColor ? &colorClear : nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = vk_helpers::attachmentInfo(drawInfo.depthTarget, drawInfo.bClearColor ? &depthClear : nullptr, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.pNext = nullptr;

    VkRenderingAttachmentInfo mrtAttachments[4];
    mrtAttachments[0] = normalAttachment;
    mrtAttachments[1] = albedoAttachment;
    mrtAttachments[2] = pbrAttachment;
    mrtAttachments[3] = velocityAttachment;

    renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, RENDER_EXTENTS};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 4;
    renderInfo.pColorAttachments = mrtAttachments;
    renderInfo.pDepthAttachment = &depthAttachment;
    renderInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(cmd, &renderInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Viewport
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = RENDER_EXTENT_WIDTH;
    viewport.height = RENDER_EXTENT_HEIGHT;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    //  Scissor
    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = RENDER_EXTENTS.width;
    scissor.extent.height = RENDER_EXTENTS.height;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkDescriptorBufferBindingInfoEXT bindingInfos[2];
    bindingInfos[0] = drawInfo.sceneDataBinding;
    bindingInfos[1] = drawInfo.environmentMapBinding;
    vkCmdBindDescriptorBuffersEXT(cmd, 2, bindingInfos);

    constexpr uint32_t sceneDataIndex{0};
    constexpr uint32_t environmentIndex{1};
    const VkDeviceSize sceneDataOffset = drawInfo.sceneDataOffset;
    const VkDeviceSize environmentMapOffset = drawInfo.environmentMapOffset;

    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &sceneDataIndex, &sceneDataOffset);
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &environmentIndex, &environmentMapOffset);

    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRendering(cmd);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void will_engine::environment_pipeline::EnvironmentPipeline::createPipeline()
{
    resourceManager.destroy(pipeline);
    VkShaderModule vertShader = resourceManager.createShaderModule("shaders/environment/environment.vert");
    VkShaderModule fragShader = resourceManager.createShaderModule("shaders/environment/environment.frag");

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.setShaders(vertShader, fragShader);
    pipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.disableMultisampling();
    pipelineBuilder.disableBlending();
    pipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipelineBuilder.setupRenderer({NORMAL_FORMAT, ALBEDO_FORMAT, PBR_FORMAT, VELOCITY_FORMAT}, DEPTH_STENCIL_FORMAT);
    pipelineBuilder.setupPipelineLayout(pipelineLayout);


    pipeline = resourceManager.createRenderPipeline(pipelineBuilder);

    resourceManager.destroy(vertShader);
    resourceManager.destroy(fragShader);
}
