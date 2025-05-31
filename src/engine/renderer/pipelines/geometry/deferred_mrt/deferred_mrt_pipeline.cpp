//
// Created by William on 2025-01-24.
//

#include "deferred_mrt_pipeline.h"

#include <array>

#include "engine/renderer/renderer_constants.h"
#include "engine/renderer/resource_manager.h"
#include "engine/renderer/assets/render_object/render_object.h"
#include "engine/renderer/assets/render_object/render_object_types.h"

namespace will_engine::renderer
{
DeferredMrtPipeline::DeferredMrtPipeline(ResourceManager& resourceManager) : resourceManager(resourceManager)
{
    std::array descriptorLayout {
        resourceManager.getSceneDataLayout(),
        resourceManager.getRenderObjectAddressesLayout(),
        resourceManager.getTexturesLayout(),
    };


    VkPipelineLayoutCreateInfo layoutInfo = vk_helpers::pipelineLayoutCreateInfo();
    layoutInfo.pNext = nullptr;
    layoutInfo.pSetLayouts = descriptorLayout.data();
    layoutInfo.setLayoutCount = descriptorLayout.size();
    layoutInfo.pPushConstantRanges = nullptr;
    layoutInfo.pushConstantRangeCount = 0;

    pipelineLayout = resourceManager.createResource<PipelineLayout>(layoutInfo);

    createPipeline();
}

DeferredMrtPipeline::~DeferredMrtPipeline()
{
    resourceManager.destroyResource(std::move(pipelineLayout));
    resourceManager.destroyResource(std::move(pipeline));
}

void DeferredMrtPipeline::draw(VkCommandBuffer cmd, const DeferredMrtDrawInfo& drawInfo) const
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Deferred MRT Pass (Render Objects)";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    constexpr VkClearValue colorClear = {.color = {0.0f, 0.0f, 0.0f, 0.0f}};
    constexpr VkClearValue depthClear = {.depthStencil = {0.0f, 0u}};

    VkRenderingAttachmentInfo normalAttachment = vk_helpers::attachmentInfo(drawInfo.normalTarget, drawInfo.bClearColor ? &colorClear : nullptr,
                                                                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo albedoAttachment = vk_helpers::attachmentInfo(drawInfo.albedoTarget, drawInfo.bClearColor ? &colorClear : nullptr,
                                                                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo pbrAttachment = vk_helpers::attachmentInfo(drawInfo.pbrTarget, drawInfo.bClearColor ? &colorClear : nullptr,
                                                                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo velocityAttachment = vk_helpers::attachmentInfo(drawInfo.velocityTarget, drawInfo.bClearColor ? &colorClear : nullptr,
                                                                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = vk_helpers::attachmentInfo(drawInfo.depthTarget, drawInfo.bClearColor ? &depthClear : nullptr,
                                                                           VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.pNext = nullptr;

    VkRenderingAttachmentInfo deferredAttachments[4];
    deferredAttachments[0] = normalAttachment;
    deferredAttachments[1] = albedoAttachment;
    deferredAttachments[2] = pbrAttachment;
    deferredAttachments[3] = velocityAttachment;

    renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, RENDER_EXTENTS};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 4;
    renderInfo.pColorAttachments = deferredAttachments;
    renderInfo.pDepthAttachment = &depthAttachment;
    renderInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(cmd, &renderInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);

    //  Viewport
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = drawInfo.viewportExtents.x;
    viewport.height = drawInfo.viewportExtents.y;
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

    for (RenderObject* renderObject : drawInfo.renderObjects) {
        if (!renderObject->canDraw()) { continue; }

        std::array descriptorBufferBindingInfos{
            drawInfo.sceneDataBinding,
            renderObject->getAddressesDescriptorBuffer()->getBindingInfo(),
            renderObject->getTextureDescriptorBuffer()->getBindingInfo(),
        };

        vkCmdBindDescriptorBuffersEXT(cmd, descriptorBufferBindingInfos.size(), descriptorBufferBindingInfos.data());

        constexpr std::array<uint32_t, 3> indices{0, 1, 2};

        std::array offsets{
            drawInfo.sceneDataOffset,
            renderObject->getAddressesDescriptorBuffer()->getDescriptorBufferSize() * drawInfo.currentFrameOverlap,
            ZERO_DEVICE_SIZE
        };

        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout->layout, 0, 3, indices.data(), offsets.data());

        const VkBuffer vertexBuffers[2] = {renderObject->getPositionVertexBuffer(), renderObject->getPropertyVertexBuffer()};
        constexpr VkDeviceSize vertexOffsets[2] = {0, 0};
        vkCmdBindVertexBuffers(cmd, 0, 2, vertexBuffers, vertexOffsets);
        vkCmdBindIndexBuffer(cmd, renderObject->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexedIndirect(cmd, renderObject->getOpaqueIndirectBuffer(drawInfo.currentFrameOverlap), 0,
                                 renderObject->getOpaqueDrawIndirectCommandCount(), sizeof(VkDrawIndexedIndirectCommand));
    }

    vkCmdEndRendering(cmd);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void DeferredMrtPipeline::createPipeline()
{
    resourceManager.destroyResource(std::move(pipeline));
    VkShaderModule vertShader = resourceManager.createShaderModule("shaders/deferredMrt.vert");
    VkShaderModule fragShader = resourceManager.createShaderModule("shaders/deferredMrt.frag");

    RenderPipelineBuilder renderPipelineBuilder;
    const std::vector<VkVertexInputBindingDescription> vertexBindings{
        {
            .binding = 0,
            .stride = sizeof(VertexPosition),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        },
        {
            .binding = 1,
            .stride = sizeof(VertexProperty),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        }
    };

    const std::vector<VkVertexInputAttributeDescription> vertexAttributes{
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(VertexPosition, position),

        },
        {
            .location = 1,
            .binding = 1,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(VertexProperty, normal),

        },
        {
            .location = 2,
            .binding = 1,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = offsetof(VertexProperty, color),

        },
        {
            .location = 3,
            .binding = 1,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(VertexProperty, uv),

        }
    };

    renderPipelineBuilder.setupVertexInput(vertexBindings, vertexAttributes);

    renderPipelineBuilder.setShaders(vertShader, fragShader);
    renderPipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    renderPipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    renderPipelineBuilder.disableMultisampling();
    renderPipelineBuilder.disableBlending();
    renderPipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    renderPipelineBuilder.setupRenderer({NORMAL_FORMAT, ALBEDO_FORMAT, PBR_FORMAT, VELOCITY_FORMAT}, DEPTH_STENCIL_FORMAT);
    renderPipelineBuilder.setupPipelineLayout(pipelineLayout->layout);
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = renderPipelineBuilder.generatePipelineCreateInfo();
    pipeline = resourceManager.createResource<Pipeline>(pipelineCreateInfo);
    resourceManager.destroyShaderModule(vertShader);
    resourceManager.destroyShaderModule(fragShader);
}
}
