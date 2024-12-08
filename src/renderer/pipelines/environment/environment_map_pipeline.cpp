//
// Created by William on 2024-12-07.
//

#include "environment_map_pipeline.h"

#include "../../vk_descriptors.h"
#include "../../vk_helpers.h"
#include "../../vk_pipelines.h"

EnvironmentPipeline::EnvironmentPipeline(VulkanContext& context)
    : context(context)
{}

EnvironmentPipeline::~EnvironmentPipeline()
{
    cleanup();
}

void EnvironmentPipeline::init(const EnvironmentPipelineCreateInfo& createInfo, const EnvironmentPipelineRenderInfo& renderInfo)
{
    sceneDataLayout = createInfo.sceneDataLayout;
    environmentMapLayout = createInfo.environmentMapLayout;
    renderFormats = renderInfo;

    createPipelineLayout();
    createPipeline();
}

void EnvironmentPipeline::createPipelineLayout()
{
    VkDescriptorSetLayout layouts[2];
    layouts[0] = sceneDataLayout;
    layouts[1] = environmentMapLayout;

    VkPipelineLayoutCreateInfo layoutInfo = vk_helpers::pipelineLayoutCreateInfo();
    layoutInfo.pNext = nullptr;
    layoutInfo.setLayoutCount = 2;
    layoutInfo.pSetLayouts = layouts;
    layoutInfo.pPushConstantRanges = nullptr;
    layoutInfo.pushConstantRangeCount = 0;

    VK_CHECK(vkCreatePipelineLayout(context.getDevice(), &layoutInfo, nullptr, &pipelineLayout));
}

void EnvironmentPipeline::createPipeline()
{
    VkShaderModule vertShader;
    if (!vk_helpers::loadShaderModule("shaders/environment.vert.spv", context.getDevice(), &vertShader)) {
        throw std::runtime_error("Error when building vertex shader (environment.vert.spv)");
    }

    VkShaderModule fragShader;
    if (!vk_helpers::loadShaderModule("shaders/environment.frag.spv", context.getDevice(), &fragShader)) {
        throw std::runtime_error("Error when building fragment shader (environment.frag.spv)");
    }

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.setShaders(vertShader, fragShader);
    pipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.disableMultisampling();
    pipelineBuilder.setupBlending(PipelineBuilder::BlendMode::NO_BLEND);
    pipelineBuilder.enableDepthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipelineBuilder.setupRenderer({renderFormats.colorFormat}, renderFormats.depthFormat);
    pipelineBuilder.setupPipelineLayout(pipelineLayout);

    pipeline = pipelineBuilder.buildPipeline(context.getDevice(), VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    vkDestroyShaderModule(context.getDevice(), vertShader, nullptr);
    vkDestroyShaderModule(context.getDevice(), fragShader, nullptr);
}

void EnvironmentPipeline::draw(VkCommandBuffer cmd, const EnvironmentDrawInfo& drawInfo) const
{
    VkDebugUtilsLabelEXT label{};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Environment Map";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    constexpr VkClearValue depthClearValue{0.0f, 0.0f};
    const VkRenderingAttachmentInfo colorAttachment = vk_helpers::attachmentInfo(drawInfo.colorAttachment, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const VkRenderingAttachmentInfo depthAttachment = vk_helpers::attachmentInfo(drawInfo.depthAttachment, &depthClearValue, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    const VkRenderingInfo renderInfo = vk_helpers::renderingInfo(drawInfo.renderExtent, &colorAttachment, &depthAttachment);

    vkCmdBeginRendering(cmd, &renderInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Viewport
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<float>(drawInfo.renderExtent.width);
    viewport.height = static_cast<float>(drawInfo.renderExtent.height);
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    //  Scissor
    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = drawInfo.renderExtent.width;
    scissor.extent.height = drawInfo.renderExtent.height;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkDescriptorBufferBindingInfoEXT bindingInfos[2];
    bindingInfos[0] = drawInfo.sceneData.getDescriptorBufferBindingInfo();
    bindingInfos[1] = drawInfo.environment->getCubemapDescriptorBuffer().getDescriptorBufferBindingInfo();
    vkCmdBindDescriptorBuffersEXT(cmd, 2, bindingInfos);

    constexpr uint32_t sceneDataIndex{0};
    constexpr uint32_t environmentIndex{1};
    constexpr VkDeviceSize zeroOffset{0};
    const VkDeviceSize environmentMapOffset = drawInfo.environment->getEnvironmentMapOffset(drawInfo.environmentMapIndex);

    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &sceneDataIndex, &zeroOffset);
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &environmentIndex, &environmentMapOffset);

    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRendering(cmd);
    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void EnvironmentPipeline::cleanup()
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
