//
// Created by William on 2024-12-07.
//

#include "deferred_mrt.h"

#include "src/renderer/vk_pipelines.h"
#include "src/renderer/vk_types.h"

DeferredMrtPipeline::DeferredMrtPipeline(VulkanContext& context)
    : context(context)
{}

DeferredMrtPipeline::~DeferredMrtPipeline()
{
    cleanup();
}

void DeferredMrtPipeline::init(const DeferredMrtPipelineCreateInfo& createInfo, const DeferredMrtPipelineRenderInfo& renderInfo)
{
    sceneDataLayout = createInfo.sceneDataLayout;
    addressesLayout = createInfo.addressesLayout;
    textureLayout = createInfo.textureLayout;
    renderFormats = renderInfo;

    createPipelineLayout();
    createPipeline();
}

void DeferredMrtPipeline::createPipelineLayout()
{
    VkDescriptorSetLayout descriptorLayout[3];
    descriptorLayout[0] = sceneDataLayout;
    descriptorLayout[1] = addressesLayout;
    descriptorLayout[2] = textureLayout;


    VkPipelineLayoutCreateInfo layoutInfo = vk_helpers::pipelineLayoutCreateInfo();
    layoutInfo.pSetLayouts = descriptorLayout;
    layoutInfo.pNext = nullptr;
    layoutInfo.setLayoutCount = 3;
    layoutInfo.pPushConstantRanges = nullptr;
    layoutInfo.pushConstantRangeCount = 0;

    VK_CHECK(vkCreatePipelineLayout(context.device, &layoutInfo, nullptr, &pipelineLayout));
}

void DeferredMrtPipeline::createPipeline()
{
    VkShaderModule vertShader;
    if (!vk_helpers::loadShaderModule("shaders/deferredMrt.vert.spv", context.device, &vertShader)) {
        throw std::runtime_error("Error when building the deferred vertex shader module(deferredMrt.vert.spv)\n");
    }
    VkShaderModule fragShader;
    if (!vk_helpers::loadShaderModule("shaders/deferredMrt.frag.spv", context.device, &fragShader)) {
        fmt::print("Error when building the deferred fragment shader module(deferredMrt.frag.spv)\n");
    }
    PipelineBuilder renderPipelineBuilder;
    VkVertexInputBindingDescription mainBinding{};
    mainBinding.binding = 0;
    mainBinding.stride = sizeof(Vertex);
    mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription vertexAttributes[5];
    vertexAttributes[0].binding = 0;
    vertexAttributes[0].location = 0;
    vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexAttributes[0].offset = offsetof(Vertex, position);

    vertexAttributes[1].binding = 0;
    vertexAttributes[1].location = 1;
    vertexAttributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexAttributes[1].offset = offsetof(Vertex, normal);

    vertexAttributes[2].binding = 0;
    vertexAttributes[2].location = 2;
    vertexAttributes[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertexAttributes[2].offset = offsetof(Vertex, color);

    vertexAttributes[3].binding = 0;
    vertexAttributes[3].location = 3;
    vertexAttributes[3].format = VK_FORMAT_R32G32_SFLOAT;
    vertexAttributes[3].offset = offsetof(Vertex, uv);

    vertexAttributes[4].binding = 0;
    vertexAttributes[4].location = 4;
    vertexAttributes[4].format = VK_FORMAT_R32_UINT;
    vertexAttributes[4].offset = offsetof(Vertex, materialIndex);

    renderPipelineBuilder.setupVertexInput(&mainBinding, 1, vertexAttributes, 5);

    renderPipelineBuilder.setShaders(vertShader, fragShader);
    renderPipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    renderPipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    renderPipelineBuilder.disableMultisampling();
    renderPipelineBuilder.setupBlending(PipelineBuilder::BlendMode::NO_BLEND);
    renderPipelineBuilder.enableDepthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    renderPipelineBuilder.setupRenderer({renderFormats.normalFormat, renderFormats.albedoFormat, renderFormats.pbrFormat, renderFormats.velocityFormat}, renderFormats.depthFormat);
    renderPipelineBuilder.setupPipelineLayout(pipelineLayout);

    pipeline = renderPipelineBuilder.buildPipeline(context.device, VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    vkDestroyShaderModule(context.device, vertShader, nullptr);
    vkDestroyShaderModule(context.device, fragShader, nullptr);
}

void DeferredMrtPipeline::draw(VkCommandBuffer cmd, DeferredMrtDrawInfo& drawInfo) const
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Deferred MRT Pass";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    VkClearValue clearValue = {0.0f, 0.0f};

    VkRenderingAttachmentInfo normalAttachment = vk_helpers::attachmentInfo(drawInfo.normalTarget, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo albedoAttachment = vk_helpers::attachmentInfo(drawInfo.albedoTarget, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo pbrAttachment = vk_helpers::attachmentInfo(drawInfo.pbrTarget, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo velocityAttachment = vk_helpers::attachmentInfo(drawInfo.velocityTarget, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = vk_helpers::attachmentInfo(drawInfo.depthTarget, &clearValue, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.pNext = nullptr;

    VkRenderingAttachmentInfo deferredAttachments[4];
    deferredAttachments[0] = normalAttachment;
    deferredAttachments[1] = albedoAttachment;
    deferredAttachments[2] = pbrAttachment;
    deferredAttachments[3] = velocityAttachment;

    renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, drawInfo.renderExtent};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 4;
    renderInfo.pColorAttachments = deferredAttachments;
    renderInfo.pDepthAttachment = &depthAttachment;
    renderInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(cmd, &renderInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    //  Viewport
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = drawInfo.viewportRenderExtent[0];
    viewport.height = drawInfo.viewportRenderExtent[1];
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

    constexpr VkDeviceSize zeroOffset{0};
    constexpr uint32_t sceneDataIndex{0};
    constexpr uint32_t addressIndex{1};
    constexpr uint32_t texturesIndex{2};

    for (RenderObject* renderObject : drawInfo.renderObjects) {
        if (!renderObject->canDraw()) { continue; }

        VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[3];
        descriptorBufferBindingInfo[0] = drawInfo.sceneData.getDescriptorBufferBindingInfo();
        descriptorBufferBindingInfo[1] = renderObject->getAddressesDescriptorBuffer().getDescriptorBufferBindingInfo();
        descriptorBufferBindingInfo[2] = renderObject->getTextureDescriptorBuffer().getDescriptorBufferBindingInfo();
        vkCmdBindDescriptorBuffersEXT(cmd, 3, descriptorBufferBindingInfo);

        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &sceneDataIndex, &zeroOffset);
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &addressIndex, &zeroOffset);
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, &texturesIndex, &zeroOffset);

        vkCmdBindVertexBuffers(cmd, 0, 1, &renderObject->getVertexBuffer().buffer, &zeroOffset);
        vkCmdBindIndexBuffer(cmd, renderObject->getIndexBuffer().buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexedIndirect(cmd, renderObject->getIndirectBuffer().buffer, 0, renderObject->getDrawIndirectCommandCount(), sizeof(VkDrawIndexedIndirectCommand));
    }

    vkCmdEndDebugUtilsLabelEXT(cmd);

    vkCmdEndRendering(cmd);
}

void DeferredMrtPipeline::cleanup()
{
    if (pipeline) {
        vkDestroyPipeline(context.device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
    if (pipelineLayout) {
        vkDestroyPipelineLayout(context.device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }
}
