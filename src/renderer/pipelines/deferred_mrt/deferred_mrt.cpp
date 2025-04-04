//
// Created by William on 2025-01-24.
//

#include "deferred_mrt.h"

#include <array>
#include <ranges>

#include "volk/volk.h"

#include "src/renderer/renderer_constants.h"
#include "src/renderer/resource_manager.h"
#include "src/renderer/assets/render_object/render_object_types.h"

will_engine::deferred_mrt::DeferredMrtPipeline::DeferredMrtPipeline(ResourceManager& resourceManager) : resourceManager(resourceManager)
{
    VkDescriptorSetLayout descriptorLayout[3];
    descriptorLayout[0] = resourceManager.getSceneDataLayout();
    descriptorLayout[1] = resourceManager.getAddressesLayout();
    descriptorLayout[2] = resourceManager.getTexturesLayout();


    VkPipelineLayoutCreateInfo layoutInfo = vk_helpers::pipelineLayoutCreateInfo();
    layoutInfo.pSetLayouts = descriptorLayout;
    layoutInfo.pNext = nullptr;
    layoutInfo.setLayoutCount = 3;
    layoutInfo.pPushConstantRanges = nullptr;
    layoutInfo.pushConstantRangeCount = 0;

    pipelineLayout = resourceManager.createPipelineLayout(layoutInfo);

    createPipeline();
}

will_engine::deferred_mrt::DeferredMrtPipeline::~DeferredMrtPipeline()
{
    resourceManager.destroyPipelineLayout(pipelineLayout);
    resourceManager.destroyPipeline(pipeline);
}

void will_engine::deferred_mrt::DeferredMrtPipeline::draw(VkCommandBuffer cmd, const DeferredMrtDrawInfo& drawInfo) const
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Deferred MRT Pass (Render Objects)";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    VkClearValue clearValue = {0.0f, 0.0f};

    VkRenderingAttachmentInfo normalAttachment = vk_helpers::attachmentInfo(drawInfo.normalTarget, drawInfo.bClearColor ? &clearValue : nullptr,
                                                                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo albedoAttachment = vk_helpers::attachmentInfo(drawInfo.albedoTarget, drawInfo.bClearColor ? &clearValue : nullptr,
                                                                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo pbrAttachment = vk_helpers::attachmentInfo(drawInfo.pbrTarget, drawInfo.bClearColor ? &clearValue : nullptr,
                                                                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo velocityAttachment = vk_helpers::attachmentInfo(drawInfo.velocityTarget, drawInfo.bClearColor ? &clearValue : nullptr,
                                                                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = vk_helpers::attachmentInfo(drawInfo.depthTarget, drawInfo.bClearColor ? &clearValue : nullptr,
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
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

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
    constexpr VkDeviceSize zeroOffset{0};

    for (RenderObject* renderObject : drawInfo.renderObjects) {
        if (!renderObject->canDraw()) { continue; }

        constexpr uint32_t sceneDataIndex{0};
        constexpr uint32_t addressIndex{1};
        constexpr uint32_t texturesIndex{2};

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

        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 3, indices.data(), offsets.data());

        vkCmdBindVertexBuffers(cmd, 0, 1, &renderObject->getVertexBuffer().buffer, &zeroOffset);
        vkCmdBindIndexBuffer(cmd, renderObject->getIndexBuffer().buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexedIndirect(cmd, renderObject->getIndirectBuffer(drawInfo.currentFrameOverlap).buffer, 0,
                                 renderObject->getDrawIndirectCommandCount(), sizeof(VkDrawIndexedIndirectCommand));
    }

    vkCmdEndRendering(cmd);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void will_engine::deferred_mrt::DeferredMrtPipeline::createPipeline()
{
    resourceManager.destroyPipeline(pipeline);
    VkShaderModule vertShader = resourceManager.createShaderModule("shaders/deferredMrt.vert");
    VkShaderModule fragShader = resourceManager.createShaderModule("shaders/deferredMrt.frag");

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
    renderPipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    renderPipelineBuilder.disableMultisampling();
    renderPipelineBuilder.setupBlending(PipelineBuilder::BlendMode::NO_BLEND);
    renderPipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    renderPipelineBuilder.setupRenderer({NORMAL_FORMAT, ALBEDO_FORMAT, PBR_FORMAT, VELOCITY_FORMAT}, DEPTH_FORMAT);
    renderPipelineBuilder.setupPipelineLayout(pipelineLayout);

    pipeline = resourceManager.createRenderPipeline(renderPipelineBuilder);
    resourceManager.destroyShaderModule(vertShader);
    resourceManager.destroyShaderModule(fragShader);
}
