//
// Created by William on 2025-02-24.
//

#include "terrain_pipeline.h"

#include <volk/volk.h>

#include "src/renderer/renderer_constants.h"
#include "src/renderer/terrain/terrain_chunk.h"
#include "src/renderer/terrain/terrain_types.h"

will_engine::terrain::TerrainPipeline::TerrainPipeline(ResourceManager& resourceManager) : resourceManager(resourceManager)
{
    VkDescriptorSetLayout descriptorLayout[1];
    descriptorLayout[0] = resourceManager.getSceneDataLayout();


    VkPushConstantRange pushConstants = {};
    pushConstants.offset = 0;
    pushConstants.size = sizeof(TerrainPushConstants);
    pushConstants.stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;

    VkPipelineLayoutCreateInfo layoutInfo = vk_helpers::pipelineLayoutCreateInfo();
    layoutInfo.pSetLayouts = descriptorLayout;
    layoutInfo.pNext = nullptr;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstants;
    layoutInfo.pushConstantRangeCount = 1;

    pipelineLayout = resourceManager.createPipelineLayout(layoutInfo);

    createPipeline();
    createLinePipeline();
}

will_engine::terrain::TerrainPipeline::~TerrainPipeline()
{
    resourceManager.destroyPipelineLayout(pipelineLayout);
    resourceManager.destroyPipeline(pipeline);
    resourceManager.destroyPipeline(linePipeline);
}

void will_engine::terrain::TerrainPipeline::draw(VkCommandBuffer cmd, const TerrainDrawInfo& drawInfo) const
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Deferred MRT Pass (Terrain)";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    VkClearValue clearValue = {0.0f, 0.0f};

    VkRenderingAttachmentInfo normalAttachment = vk_helpers::attachmentInfo(drawInfo.normalTarget, drawInfo.bClearColor ? &clearValue : nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo albedoAttachment = vk_helpers::attachmentInfo(drawInfo.albedoTarget, drawInfo.bClearColor ? &clearValue : nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo pbrAttachment = vk_helpers::attachmentInfo(drawInfo.pbrTarget, drawInfo.bClearColor ? &clearValue : nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo velocityAttachment = vk_helpers::attachmentInfo(drawInfo.velocityTarget, drawInfo.bClearColor ? &clearValue : nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = vk_helpers::attachmentInfo(drawInfo.depthTarget, drawInfo.bClearColor ? &clearValue : nullptr, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

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
    if (drawInfo.bDrawLinesOnly) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline);
    }
    else {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    }


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

    TerrainPushConstants push{1.0f};
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 0, sizeof(TerrainPushConstants), &push);

    constexpr VkDeviceSize zeroOffset{0};

    for (ITerrain* terrain : drawInfo.terrains) {
        if (!terrain->canDraw()) { continue; }

        constexpr uint32_t sceneDataIndex{0};

        VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[1];
        descriptorBufferBindingInfo[0] = drawInfo.sceneDataBinding;
        vkCmdBindDescriptorBuffersEXT(cmd, 1, descriptorBufferBindingInfo);

        const VkDeviceSize sceneDataOffset{drawInfo.sceneDataOffset};

        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &sceneDataIndex, &sceneDataOffset);

        VkBuffer vertexBuffer = terrain->getVertexBuffer().buffer;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &zeroOffset);
        vkCmdBindIndexBuffer(cmd, terrain->getIndexBuffer().buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, terrain->getIndicesCount(), 1, 0, 0, 0);
    }

    vkCmdEndRendering(cmd);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void will_engine::terrain::TerrainPipeline::createPipeline()
{
    resourceManager.destroyPipeline(pipeline);
    VkShaderModule vertShader = resourceManager.createShaderModule("shaders/terrain/terrain.vert");
    VkShaderModule tescShader = resourceManager.createShaderModule("shaders/terrain/terrain.tesc");
    VkShaderModule teseShader = resourceManager.createShaderModule("shaders/terrain/terrain.tese");
    VkShaderModule fragShader = resourceManager.createShaderModule("shaders/terrain/terrain.frag");

    PipelineBuilder pipelineBuilder;
    VkVertexInputBindingDescription mainBinding{};
    mainBinding.binding = 0;
    mainBinding.stride = sizeof(TerrainVertex);
    mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription vertexAttributes[3];
    vertexAttributes[0].binding = 0;
    vertexAttributes[0].location = 0;
    vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexAttributes[0].offset = offsetof(TerrainVertex, position);
    vertexAttributes[1].binding = 0;
    vertexAttributes[1].location = 1;
    vertexAttributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexAttributes[1].offset = offsetof(TerrainVertex, normal);
    vertexAttributes[2].binding = 0;
    vertexAttributes[2].location = 2;
    vertexAttributes[2].format = VK_FORMAT_R32G32_SFLOAT;
    vertexAttributes[2].offset = offsetof(TerrainVertex, uv);

    pipelineBuilder.setupVertexInput(&mainBinding, 1, vertexAttributes, 3);

    pipelineBuilder.setShaders(vertShader, tescShader, teseShader, fragShader);
    pipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, false);
    pipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.disableMultisampling();
    pipelineBuilder.setupBlending(PipelineBuilder::BlendMode::NO_BLEND);
    pipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipelineBuilder.setupRenderer({NORMAL_FORMAT, ALBEDO_FORMAT, PBR_FORMAT, VELOCITY_FORMAT}, DEPTH_FORMAT);
    pipelineBuilder.setupPipelineLayout(pipelineLayout);
    pipelineBuilder.setupTessellation(4);

    pipeline = resourceManager.createRenderPipeline(pipelineBuilder, {VK_DYNAMIC_STATE_DEPTH_BIAS});
    resourceManager.destroyShaderModule(vertShader);
    resourceManager.destroyShaderModule(tescShader);
    resourceManager.destroyShaderModule(teseShader);
    resourceManager.destroyShaderModule(fragShader);
}

void will_engine::terrain::TerrainPipeline::createLinePipeline()
{
    resourceManager.destroyPipeline(linePipeline);
    VkShaderModule vertShader = resourceManager.createShaderModule("shaders/terrain/terrain.vert");
    VkShaderModule tescShader = resourceManager.createShaderModule("shaders/terrain/terrain.tesc");
    VkShaderModule teseShader = resourceManager.createShaderModule("shaders/terrain/terrain.tese");
    VkShaderModule fragShader = resourceManager.createShaderModule("shaders/terrain/terrain.frag");

    PipelineBuilder pipelineBuilder;
    VkVertexInputBindingDescription mainBinding{};
    mainBinding.binding = 0;
    mainBinding.stride = sizeof(TerrainVertex);
    mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription vertexAttributes[3];
    vertexAttributes[0].binding = 0;
    vertexAttributes[0].location = 0;
    vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexAttributes[0].offset = offsetof(TerrainVertex, position);
    vertexAttributes[1].binding = 0;
    vertexAttributes[1].location = 1;
    vertexAttributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexAttributes[1].offset = offsetof(TerrainVertex, normal);
    vertexAttributes[2].binding = 0;
    vertexAttributes[2].location = 2;
    vertexAttributes[2].format = VK_FORMAT_R32G32_SFLOAT;
    vertexAttributes[2].offset = offsetof(TerrainVertex, uv);

    pipelineBuilder.setupVertexInput(&mainBinding, 1, vertexAttributes, 3);

    pipelineBuilder.setShaders(vertShader, tescShader, teseShader, fragShader);
    pipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, false);
    pipelineBuilder.setupRasterization(VK_POLYGON_MODE_LINE, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.disableMultisampling();
    pipelineBuilder.setupBlending(PipelineBuilder::BlendMode::NO_BLEND);
    pipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipelineBuilder.setupRenderer({NORMAL_FORMAT, ALBEDO_FORMAT, PBR_FORMAT, VELOCITY_FORMAT}, DEPTH_FORMAT);
    pipelineBuilder.setupPipelineLayout(pipelineLayout);
    pipelineBuilder.setupTessellation(4);

    linePipeline = resourceManager.createRenderPipeline(pipelineBuilder, {VK_DYNAMIC_STATE_DEPTH_BIAS});
    resourceManager.destroyShaderModule(vertShader);
    resourceManager.destroyShaderModule(tescShader);
    resourceManager.destroyShaderModule(teseShader);
    resourceManager.destroyShaderModule(fragShader);
}
