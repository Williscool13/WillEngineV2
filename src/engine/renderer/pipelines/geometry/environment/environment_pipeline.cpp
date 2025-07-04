//
// Created by William on 2025-01-25.
//

#include "environment_pipeline.h"

#include "engine/renderer/resource_manager.h"
#include "engine/renderer/vk_helpers.h"
#include "engine/renderer/vk_pipelines.h"
#include "engine/renderer/resources/pipeline.h"
#include "engine/renderer/resources/pipeline_layout.h"
#include "engine/renderer/resources/shader_module.h"

namespace will_engine::renderer
{
EnvironmentPipeline::EnvironmentPipeline(ResourceManager& resourceManager, VkDescriptorSetLayout environmentMapLayout) : resourceManager(
    resourceManager)
{
    std::array layouts{
        resourceManager.getSceneDataLayout(),
        environmentMapLayout,
    };

    VkPushConstantRange pushConstants = {};
    pushConstants.offset = 0;
    pushConstants.size = sizeof(pushConstants);
    pushConstants.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo layoutInfo = vk_helpers::pipelineLayoutCreateInfo();
    layoutInfo.pNext = nullptr;
    layoutInfo.pSetLayouts = layouts.data();
    layoutInfo.setLayoutCount = layouts.size();
    layoutInfo.pPushConstantRanges = &pushConstants;
    layoutInfo.pushConstantRangeCount = 1;

    pipelineLayout = resourceManager.createResource<PipelineLayout>(layoutInfo);

    createPipeline();
}

EnvironmentPipeline::~EnvironmentPipeline()
{
    resourceManager.destroyResource(std::move(pipeline));
    resourceManager.destroyResource(std::move(pipelineLayout));
}

void EnvironmentPipeline::draw(VkCommandBuffer cmd, const EnvironmentDrawInfo& drawInfo) const
{
    VkDebugUtilsLabelEXT label{};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Environment Map";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    VkRenderingAttachmentInfo normalAttachment = vk_helpers::attachmentInfo(drawInfo.normalTarget, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo albedoAttachment = vk_helpers::attachmentInfo(drawInfo.albedoTarget, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo pbrAttachment = vk_helpers::attachmentInfo(drawInfo.pbrTarget, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo velocityAttachment = vk_helpers::attachmentInfo(drawInfo.velocityTarget, nullptr,
                                                                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = vk_helpers::attachmentInfo(drawInfo.depthTarget, nullptr, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.pNext = nullptr;

    VkRenderingAttachmentInfo mrtAttachments[4];
    mrtAttachments[0] = normalAttachment;
    mrtAttachments[1] = albedoAttachment;
    mrtAttachments[2] = pbrAttachment;
    mrtAttachments[3] = velocityAttachment;

    renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, drawInfo.renderExtents};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 4;
    renderInfo.pColorAttachments = mrtAttachments;
    renderInfo.pDepthAttachment = &depthAttachment;
    renderInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(cmd, &renderInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);

    // Viewport
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = drawInfo.renderExtents.width;
    viewport.height = drawInfo.renderExtents.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    //  Scissor
    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = drawInfo.renderExtents.width;
    scissor.extent.height = drawInfo.renderExtents.height;
    vkCmdSetScissor(cmd, 0, 1, &scissor);


    vkCmdPushConstants(cmd, pipelineLayout->layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(EnvironmentPushConstants), &pushConstants);

    std::array bindingInfos{drawInfo.sceneDataBinding, drawInfo.environmentMapBinding};

    constexpr std::array<uint32_t, 2> indices{0, 1};
    const std::array offsets{drawInfo.sceneDataOffset, drawInfo.environmentMapOffset};

    vkCmdBindDescriptorBuffersEXT(cmd, bindingInfos.size(), bindingInfos.data());
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout->layout, 0, 2, indices.data(), offsets.data());

    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRendering(cmd);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void EnvironmentPipeline::createPipeline()
{
    resourceManager.destroyResource(std::move(pipeline));
    ShaderModulePtr vertShader = resourceManager.createResource<ShaderModule>("shaders/environment/environment.vert");
    ShaderModulePtr fragShader = resourceManager.createResource<ShaderModule>("shaders/environment/environment.frag");

    RenderPipelineBuilder pipelineBuilder;
    pipelineBuilder.setShaders(vertShader->shader, fragShader->shader);
    pipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.disableMultisampling();
    pipelineBuilder.disableBlending();
    pipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipelineBuilder.setupRenderer({NORMAL_FORMAT, ALBEDO_FORMAT, PBR_FORMAT, VELOCITY_FORMAT}, DEPTH_STENCIL_FORMAT);
    pipelineBuilder.setupPipelineLayout(pipelineLayout->layout);
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = pipelineBuilder.generatePipelineCreateInfo();
    pipeline = resourceManager.createResource<Pipeline>(pipelineCreateInfo);
}
}
