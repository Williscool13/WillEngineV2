//
// Created by William on 2025-05-04.
//

#include "debug_highlighter.h"

#include <volk/volk.h>

#include "engine/core/game_object/renderable.h"
#include "engine/renderer/assets/render_object/render_object_types.h"


namespace will_engine::debug_highlight_pipeline
{
DebugHighlighter::DebugHighlighter(ResourceManager& resourceManager) : resourceManager(resourceManager)
{
    std::array<VkDescriptorSetLayout, 2> descriptorLayout;
    descriptorLayout[0] = resourceManager.getSceneDataLayout();

    VkPushConstantRange pushConstants{};
    pushConstants.offset = 0;
    pushConstants.size = sizeof(DebugHighlightDrawPushConstant);
    pushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo layoutInfo = vk_helpers::pipelineLayoutCreateInfo();
    layoutInfo.pNext = nullptr;
    layoutInfo.pSetLayouts = descriptorLayout.data();
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstants;
    layoutInfo.pushConstantRangeCount = 1;

    pipelineLayout = resourceManager.createPipelineLayout(layoutInfo);

    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // Stencil Image
    layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // Debug Target

    processingSetLayout = resourceManager.createDescriptorSetLayout(layoutBuilder, VK_SHADER_STAGE_COMPUTE_BIT,
                                                                    VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    descriptorLayout[1] = processingSetLayout;

    layoutInfo.setLayoutCount = 2;
    layoutInfo.pPushConstantRanges = nullptr;
    layoutInfo.pushConstantRangeCount = 0;

    processingPipelineLayout = resourceManager.createPipelineLayout(layoutInfo);

    createPipeline();

    descriptorBuffer = resourceManager.createDescriptorBufferSampler(processingSetLayout, 1);
}

DebugHighlighter::~DebugHighlighter()
{
    resourceManager.destroy(processingSetLayout);
    resourceManager.destroy(pipelineLayout);
    resourceManager.destroy(processingPipelineLayout);
    resourceManager.destroy(pipeline);
    resourceManager.destroy(processingPipeline);
    resourceManager.destroy(descriptorBuffer);
}

void DebugHighlighter::setupDescriptorBuffer(VkImageView stencilImageView, VkImageView debugTarget)
{
    std::vector<DescriptorImageData> descriptors;
    descriptors.reserve(2);

    VkDescriptorImageInfo inputImage{};
    inputImage.imageView = stencilImageView;
    inputImage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo outputImage{};
    outputImage.imageView = debugTarget;
    outputImage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    descriptors.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, inputImage, false});
    descriptors.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, outputImage, false});

    resourceManager.setupDescriptorBufferSampler(descriptorBuffer, descriptors, 0);
}

bool DebugHighlighter::drawHighlightStencil(VkCommandBuffer cmd, const DebugHighlighterDrawInfo& drawInfo) const
{
    if (!drawInfo.highlightTarget->canDrawHighlight()) {
        return false;
    }

    HighlightData highlightData = drawInfo.highlightTarget->getHighlightData();


    const VkRenderingAttachmentInfo depthAttachment = vk_helpers::attachmentInfo(drawInfo.depthStencilTarget.imageView, nullptr,
                                                                                 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    VkClearValue stencilClear{};
    stencilClear.depthStencil = {0.0f, 0};
    const VkRenderingAttachmentInfo stencilAttachment = vk_helpers::attachmentInfo(drawInfo.depthStencilTarget.imageView, &stencilClear,
                                                                                   VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.pNext = nullptr;

    renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, RENDER_EXTENTS};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 0;
    renderInfo.pColorAttachments = nullptr;
    renderInfo.pDepthAttachment = &depthAttachment;
    renderInfo.pStencilAttachment = &stencilAttachment;

    vkCmdBeginRendering(cmd, &renderInfo);

    vkCmdSetLineWidth(cmd, 2.0f);

    //  Viewport
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = RENDER_EXTENTS.width;
    viewport.height = RENDER_EXTENTS.height;
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

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    const std::array bindingInfos{
        drawInfo.sceneDataBinding,
    };
    vkCmdBindDescriptorBuffersEXT(cmd, 1, bindingInfos.data());

    constexpr std::array indices{0u};
    const std::array offsets{drawInfo.sceneDataOffset};
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, indices.data(), offsets.data());

    DebugHighlightDrawPushConstant push{};
    push.modelMatrix = highlightData.modelMatrix;
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(DebugHighlightDrawPushConstant), &push);

    vkCmdBindVertexBuffers(cmd, 0, 1, &highlightData.vertexBuffer->buffer, &ZERO_DEVICE_SIZE);
    vkCmdBindIndexBuffer(cmd, highlightData.indexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);

    for (const Primitive& primitive : highlightData.primitives) {
        const uint32_t indexCount = primitive.indexCount;
        const uint32_t firstIndex = primitive.firstIndex;
        const int32_t vertexOffset = primitive.vertexOffset;
        constexpr uint32_t firstInstance = 0;
        constexpr uint32_t instanceCount = 1;

        // Draw this mesh w/ vkCmdDrawIndexed w/ model matrix passed through push
        vkCmdDrawIndexed(cmd, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    vkCmdEndRendering(cmd);
    return true;

    return false;
}

void DebugHighlighter::drawHighlightProcessing(VkCommandBuffer cmd, const DebugHighlighterDrawInfo& drawInfo) const
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, processingPipeline);

    const std::array bindingInfos{
        drawInfo.sceneDataBinding,
        descriptorBuffer.getDescriptorBufferBindingInfo()
    };
    vkCmdBindDescriptorBuffersEXT(cmd, 2, bindingInfos.data());

    constexpr std::array<uint32_t, 2> indices{0, 1};
    const std::array<VkDeviceSize, 2> offsets{drawInfo.sceneDataOffset, 0};
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, processingPipelineLayout, 0, 2, indices.data(), offsets.data());

    const auto x = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_WIDTH / 16.0f));
    const auto y = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_HEIGHT / 16.0f));
    vkCmdDispatch(cmd, x, y, 1);
}


void DebugHighlighter::createPipeline()
{
    VkShaderModule vertShader = resourceManager.createShaderModule("shaders/debug/debug_highlighter.vert");
    VkShaderModule fragShader = resourceManager.createShaderModule("shaders/debug/debug_highlighter.frag");

    PipelineBuilder renderPipelineBuilder;
    VkVertexInputBindingDescription vertexBinding{};
    vertexBinding.binding = 0;
    vertexBinding.stride = sizeof(VertexPosition);
    vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 1> vertexAttributes;
    vertexAttributes[0].binding = 0;
    vertexAttributes[0].location = 0;
    vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexAttributes[0].offset = offsetof(VertexPosition, position);

    renderPipelineBuilder.setupVertexInput(&vertexBinding, 1, vertexAttributes.data(), 1);

    renderPipelineBuilder.setShaders(vertShader, fragShader);
    renderPipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    renderPipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    renderPipelineBuilder.disableMultisampling();
    renderPipelineBuilder.disableBlending();
    constexpr VkStencilOpState stencilOp = {
        VK_STENCIL_OP_REPLACE,
        VK_STENCIL_OP_REPLACE,
        VK_STENCIL_OP_REPLACE,
        VK_COMPARE_OP_ALWAYS,
        0xff,
        0xff,
        1
    };
    renderPipelineBuilder.setupDepthStencil(VK_TRUE, VK_TRUE, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_FALSE, VK_TRUE, stencilOp, stencilOp, 0.0f, 1.0f);
    renderPipelineBuilder.setupRenderer({}, DEPTH_STENCIL_FORMAT, DEPTH_STENCIL_FORMAT);
    renderPipelineBuilder.setupPipelineLayout(pipelineLayout);
    const std::vector additionalDynamicStates{VK_DYNAMIC_STATE_LINE_WIDTH};
    pipeline = resourceManager.createRenderPipeline(renderPipelineBuilder, additionalDynamicStates);

    resourceManager.destroy(vertShader);
    resourceManager.destroy(fragShader);


    resourceManager.destroy(processingPipeline);
    VkShaderModule computeShader = resourceManager.createShaderModule("shaders/debug/debug_highlighter_processing.comp");

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = computeShader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.layout = processingPipelineLayout;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    processingPipeline = resourceManager.createComputePipeline(pipelineInfo);
    resourceManager.destroy(computeShader);
}
}
