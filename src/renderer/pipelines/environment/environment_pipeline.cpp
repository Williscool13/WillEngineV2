//
// Created by William on 2025-01-25.
//

#include "environment_pipeline.h"

#include "volk.h"
#include "src/renderer/renderer_constants.h"
#include "src/renderer/resource_manager.h"

will_engine::environment::EnvironmentPipeline::EnvironmentPipeline(ResourceManager* _resourceManager, VkDescriptorSetLayout environmentMapLayout) : resourceManager(_resourceManager)
{
    if (!resourceManager) { return; }

    VkDescriptorSetLayout layouts[2];
    layouts[0] = resourceManager->getSceneDataLayout();
    layouts[1] = environmentMapLayout;

    VkPipelineLayoutCreateInfo layoutInfo = vk_helpers::pipelineLayoutCreateInfo();
    layoutInfo.pNext = nullptr;
    layoutInfo.setLayoutCount = 2;
    layoutInfo.pSetLayouts = layouts;
    layoutInfo.pPushConstantRanges = nullptr;
    layoutInfo.pushConstantRangeCount = 0;

    pipelineLayout = resourceManager->createPipelineLayout(layoutInfo);


    VkShaderModule vertShader = resourceManager->createShaderModule("shaders/environment/environment.vert.spv");
    VkShaderModule fragShader = resourceManager->createShaderModule("shaders/environment/environment.frag.spv");

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.setShaders(vertShader, fragShader);
    pipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.disableMultisampling();
    pipelineBuilder.setupBlending(PipelineBuilder::BlendMode::NO_BLEND);
    pipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipelineBuilder.setupRenderer({DRAW_FORMAT}, DEPTH_FORMAT);
    pipelineBuilder.setupPipelineLayout(pipelineLayout);


    pipeline = resourceManager->createRenderPipeline(pipelineBuilder);

    resourceManager->destroyShaderModule(vertShader);
    resourceManager->destroyShaderModule(fragShader);
}

will_engine::environment::EnvironmentPipeline::~EnvironmentPipeline()
{
    if (!resourceManager) { return; }

    resourceManager->destroyPipeline(pipeline);
    resourceManager->destroyPipelineLayout(pipelineLayout);
}

void will_engine::environment::EnvironmentPipeline::draw(VkCommandBuffer cmd, const EnvironmentDrawInfo& drawInfo) const
{
VkDebugUtilsLabelEXT label{};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Environment Map";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    constexpr VkClearValue depthClearValue{0.0f, 0.0f};
    const VkRenderingAttachmentInfo colorAttachment = vk_helpers::attachmentInfo(drawInfo.colorAttachment, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const VkRenderingAttachmentInfo depthAttachment = vk_helpers::attachmentInfo(drawInfo.depthAttachment, &depthClearValue, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    const VkRenderingInfo renderInfo = vk_helpers::renderingInfo(RENDER_EXTENTS, &colorAttachment, &depthAttachment);

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
