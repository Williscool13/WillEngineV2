//
// Created by William on 2025-05-04.
//

#include "debug_highlighter.h"

#include <array>
#include <volk/volk.h>

#include "engine/core/game_object/renderable.h"
#include "engine/renderer/renderer_constants.h"
#include "engine/renderer/resource_manager.h"
#include "engine/renderer/vk_descriptors.h"
#include "engine/renderer/vk_helpers.h"
#include "engine/renderer/assets/render_object/render_object_types.h"


namespace will_engine::renderer
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

    pipelineLayout = resourceManager.createResource<PipelineLayout>(layoutInfo);




    DescriptorLayoutBuilder layoutBuilder{2};
    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // Stencil Image
    layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // Debug Target

    VkDescriptorSetLayoutCreateInfo createInfo = layoutBuilder.build(
        VK_SHADER_STAGE_COMPUTE_BIT,
        VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    processingSetLayout = resourceManager.createResource<DescriptorSetLayout>(createInfo);

    descriptorLayout[1] = processingSetLayout->layout;

    layoutInfo.setLayoutCount = 2;
    layoutInfo.pPushConstantRanges = nullptr;
    layoutInfo.pushConstantRangeCount = 0;

    processingPipelineLayout = resourceManager.createResource<PipelineLayout>(layoutInfo);

    createPipeline();

    descriptorBuffer = resourceManager.createResource<DescriptorBufferSampler>(processingSetLayout->layout, 1);
}

DebugHighlighter::~DebugHighlighter()
{
    resourceManager.destroyResource(std::move(processingSetLayout));
    resourceManager.destroyResource(std::move(pipelineLayout));
    resourceManager.destroyResource(std::move(processingPipelineLayout));
    resourceManager.destroyResource(std::move(pipeline));
    resourceManager.destroyResource(std::move(processingPipeline));
    resourceManager.destroyResource(std::move(descriptorBuffer));
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

    descriptorBuffer->setupData(descriptors, 0);
}

bool DebugHighlighter::drawHighlightStencil(VkCommandBuffer cmd, const renderer::DebugHighlighterDrawInfo& drawInfo) const
{
    if (!drawInfo.highlightTarget->canDrawHighlight()) {
        return false;
    }

    HighlightData highlightData = drawInfo.highlightTarget->getHighlightData();


    const VkRenderingAttachmentInfo depthAttachment = vk_helpers::attachmentInfo(drawInfo.depthStencilTarget, nullptr,
                                                                                 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    VkClearValue stencilClear{};
    stencilClear.depthStencil = {0.0f, 0};
    const VkRenderingAttachmentInfo stencilAttachment = vk_helpers::attachmentInfo(drawInfo.depthStencilTarget, &stencilClear,
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

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);

    const std::array bindingInfos{
        drawInfo.sceneDataBinding,
    };
    vkCmdBindDescriptorBuffersEXT(cmd, 1, bindingInfos.data());

    constexpr std::array indices{0u};
    const std::array offsets{drawInfo.sceneDataOffset};
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout->layout, 0, 1, indices.data(), offsets.data());

    DebugHighlightDrawPushConstant push{};
    push.modelMatrix = highlightData.modelMatrix;
    vkCmdPushConstants(cmd, pipelineLayout->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(DebugHighlightDrawPushConstant), &push);

    vkCmdBindVertexBuffers(cmd, 0, 1, &highlightData.vertexBuffer, &ZERO_DEVICE_SIZE);
    vkCmdBindIndexBuffer(cmd, highlightData.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

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
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, processingPipeline->pipeline);

    const std::array bindingInfos{
        drawInfo.sceneDataBinding,
        descriptorBuffer->getBindingInfo()
    };
    vkCmdBindDescriptorBuffersEXT(cmd, 2, bindingInfos.data());

    constexpr std::array<uint32_t, 2> indices{0, 1};
    const std::array<VkDeviceSize, 2> offsets{drawInfo.sceneDataOffset, 0};
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, processingPipelineLayout->layout, 0, 2, indices.data(), offsets.data());

    const auto x = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_WIDTH / 16.0f));
    const auto y = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_HEIGHT / 16.0f));
    vkCmdDispatch(cmd, x, y, 1);
}


void DebugHighlighter::createPipeline()
{
    VkShaderModule vertShader = resourceManager.createShaderModule("shaders/debug/debug_highlighter.vert");
    VkShaderModule fragShader = resourceManager.createShaderModule("shaders/debug/debug_highlighter.frag");

    RenderPipelineBuilder renderPipelineBuilder;
    const std::vector<VkVertexInputBindingDescription> vertexBindings{
        {
            .binding = 0,
            .stride = sizeof(VertexPosition),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        }
    };

    const std::vector<VkVertexInputAttributeDescription> vertexAttributes{
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(VertexPosition, position),
        }
    };

    renderPipelineBuilder.setupVertexInput(vertexBindings, vertexAttributes);

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
    renderPipelineBuilder.setupPipelineLayout(pipelineLayout->layout);
    renderPipelineBuilder.addDynamicState(VK_DYNAMIC_STATE_LINE_WIDTH);
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = renderPipelineBuilder.generatePipelineCreateInfo();
    pipeline = resourceManager.createResource<Pipeline>(pipelineCreateInfo);

    resourceManager.destroyShaderModule(vertShader);
    resourceManager.destroyShaderModule(fragShader);


    resourceManager.destroyResource(std::move(processingPipeline));
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
    pipelineInfo.layout = processingPipelineLayout->layout;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    processingPipeline = resourceManager.createResource<Pipeline>(pipelineInfo);
    resourceManager.destroyShaderModule(computeShader);
}
}
