//
// Created by William on 2025-04-06.
//

#include "transparent_pipeline.h"

#include <array>
#include <volk/volk.h>

#include "src/renderer/renderer_constants.h"
#include "src/renderer/assets/render_object/render_object.h"
#include "src/renderer/assets/render_object/render_object_types.h"

namespace will_engine::transparent_pipeline
{
TransparentPipeline::TransparentPipeline(ResourceManager& resourceManager): resourceManager(resourceManager)
{
    VkDescriptorSetLayout descriptorLayout[3];
    descriptorLayout[0] = resourceManager.getSceneDataLayout();
    descriptorLayout[1] = resourceManager.getRenderObjectAddressesLayout();
    descriptorLayout[2] = resourceManager.getTexturesLayout();


    VkPipelineLayoutCreateInfo layoutInfo = vk_helpers::pipelineLayoutCreateInfo();
    layoutInfo.pSetLayouts = descriptorLayout;
    layoutInfo.pNext = nullptr;
    layoutInfo.setLayoutCount = 3;
    layoutInfo.pPushConstantRanges = nullptr;
    layoutInfo.pushConstantRangeCount = 0;

    accumulationPipelineLayout = resourceManager.createPipelineLayout(layoutInfo);

    VkImageUsageFlags usage{};
    usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    const VkImageCreateInfo imgInfo = vk_helpers::imageCreateInfo(accumulationImageFormat, usage, {RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1});
    accumulationImage = resourceManager.createImage(imgInfo);

    const VkImageCreateInfo revealageImgInfo = vk_helpers::imageCreateInfo(revealageImageFormat, usage, {
                                                                               RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1
                                                                           });
    revealageImage = resourceManager.createImage(revealageImgInfo);

    const VkImageCreateInfo debugImgInfo = vk_helpers::imageCreateInfo(debugImageFormat, usage, {RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1});
    debugImage = resourceManager.createImage(debugImgInfo);

    createAccumulationPipeline();
}

TransparentPipeline::~TransparentPipeline()
{
    resourceManager.destroyImage(accumulationImage);
    resourceManager.destroyImage(revealageImage);
    resourceManager.destroyImage(debugImage);
    resourceManager.destroyPipelineLayout(accumulationPipelineLayout);
    resourceManager.destroyPipeline(accumulationPipeline);
}

void TransparentPipeline::draw(VkCommandBuffer cmd, const TransparentDrawInfo& drawInfo) const
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Transparent Forward Pass (Render Objects)";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    vk_helpers::transitionImage(cmd, accumulationImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, revealageImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, debugImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                VK_IMAGE_ASPECT_COLOR_BIT);

    VkClearValue clearValue = {0.0f, 0.0f};

    VkRenderingAttachmentInfo accumulationAttachment = vk_helpers::attachmentInfo(accumulationImage.imageView, &clearValue,
                                                                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo revealageAttachment = vk_helpers::attachmentInfo(revealageImage.imageView, &clearValue,
                                                                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo debugAttachment = vk_helpers::attachmentInfo(debugImage.imageView, &clearValue,
                                                                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingAttachmentInfo depthAttachment = vk_helpers::attachmentInfo(drawInfo.depthTarget, nullptr,
                                                                           VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.pNext = nullptr;

    VkRenderingAttachmentInfo transparentAttachments[3];
    transparentAttachments[0] = accumulationAttachment;
    transparentAttachments[1] = revealageAttachment;
    transparentAttachments[2] = debugAttachment;

    renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, RENDER_EXTENTS};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 3;
    renderInfo.pColorAttachments = transparentAttachments;
    renderInfo.pDepthAttachment = &depthAttachment;
    renderInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(cmd, &renderInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, accumulationPipeline);

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
    constexpr VkDeviceSize zeroOffset{0};

    for (RenderObject* renderObject : drawInfo.renderObjects) {
        if (!renderObject->canDrawTransparent()) { continue; }

        std::array descriptorBufferBindingInfos{
            drawInfo.sceneDataBinding,
            renderObject->getAddressesDescriptorBuffer().getDescriptorBufferBindingInfo(),
            renderObject->getTextureDescriptorBuffer().getDescriptorBufferBindingInfo(),
        };

        vkCmdBindDescriptorBuffersEXT(cmd, 3, descriptorBufferBindingInfos.data());

        constexpr std::array<uint32_t, 3> indices{0, 1, 2};

        std::array offsets{
            drawInfo.sceneDataOffset,
            renderObject->getAddressesDescriptorBuffer().getDescriptorBufferSize() * drawInfo.currentFrameOverlap,
            ZERO_DEVICE_SIZE
        };

        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, accumulationPipelineLayout, 0, 3, indices.data(), offsets.data());

        vkCmdBindVertexBuffers(cmd, 0, 1, &renderObject->getVertexBuffer().buffer, &zeroOffset);
        vkCmdBindIndexBuffer(cmd, renderObject->getIndexBuffer().buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexedIndirect(cmd, renderObject->getTransparentIndirectBuffer(drawInfo.currentFrameOverlap).buffer, 0,
                                 renderObject->getTransparentDrawIndirectCommandCount(), sizeof(VkDrawIndexedIndirectCommand));
    }

    vkCmdEndRendering(cmd);

    vk_helpers::transitionImage(cmd, accumulationImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, revealageImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, debugImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_IMAGE_ASPECT_COLOR_BIT);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void TransparentPipeline::createAccumulationPipeline()
{
    resourceManager.destroyPipeline(accumulationPipeline);
    VkShaderModule vertShader = resourceManager.createShaderModule("shaders/transparent.vert");
    VkShaderModule fragShader = resourceManager.createShaderModule("shaders/transparent.frag");

    PipelineBuilder pipelineBuilder;
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

    pipelineBuilder.setupVertexInput(&mainBinding, 1, vertexAttributes, 4);

    pipelineBuilder.setShaders(vertShader, fragShader);
    pipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.disableMultisampling();

    std::vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates{3};
    blendAttachmentStates[0].blendEnable = VK_TRUE;
    blendAttachmentStates[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachmentStates[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachmentStates[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachmentStates[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachmentStates[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachmentStates[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendAttachmentStates[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    blendAttachmentStates[1] = blendAttachmentStates[0];
    blendAttachmentStates[2] = blendAttachmentStates[0];

    pipelineBuilder.setupBlending(blendAttachmentStates);
    pipelineBuilder.enableDepthTest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipelineBuilder.setupRenderer({accumulationImageFormat, revealageImageFormat, debugImageFormat}, DEPTH_FORMAT);
    pipelineBuilder.setupPipelineLayout(accumulationPipelineLayout);

    accumulationPipeline = resourceManager.createRenderPipeline(pipelineBuilder);
    resourceManager.destroyShaderModule(vertShader);
    resourceManager.destroyShaderModule(fragShader);
}
}
