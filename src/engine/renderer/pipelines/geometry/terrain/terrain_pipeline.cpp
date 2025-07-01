//
// Created by William on 2025-02-24.
//

#include "terrain_pipeline.h"

#include "engine/core/game_object/terrain.h"
#include "engine/renderer/resource_manager.h"
#include "engine/renderer/vk_helpers.h"
#include "engine/renderer/vk_pipelines.h"
#include "engine/renderer/resources/pipeline.h"
#include "engine/renderer/resources/pipeline_layout.h"
#include "engine/renderer/resources/shader_module.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_sampler.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_uniform.h"
#include "engine/renderer/terrain/terrain_chunk.h"
#include "engine/renderer/terrain/terrain_types.h"

namespace will_engine::renderer
{
TerrainPipeline::TerrainPipeline(ResourceManager& resourceManager) : resourceManager(resourceManager)
{
    std::array descriptorLayout{
        resourceManager.getSceneDataLayout(),
        resourceManager.getTerrainTexturesLayout(),
        resourceManager.getTerrainUniformLayout(),
    };


    VkPushConstantRange pushConstants = {};
    pushConstants.offset = 0;
    pushConstants.size = sizeof(TerrainPushConstants);
    pushConstants.stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;

    VkPipelineLayoutCreateInfo layoutInfo = vk_helpers::pipelineLayoutCreateInfo();
    layoutInfo.pNext = nullptr;
    layoutInfo.pSetLayouts = descriptorLayout.data();
    layoutInfo.setLayoutCount = descriptorLayout.size();
    layoutInfo.pPushConstantRanges = &pushConstants;
    layoutInfo.pushConstantRangeCount = 1;

    pipelineLayout = resourceManager.createResource<PipelineLayout>(layoutInfo);

    createPipeline();
}

TerrainPipeline::~TerrainPipeline()
{
    resourceManager.destroyResource(std::move(pipelineLayout));
    resourceManager.destroyResource(std::move(pipeline));
}

void TerrainPipeline::draw(VkCommandBuffer cmd, const TerrainDrawInfo& drawInfo) const
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Deferred MRT Pass (Terrain)";
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

    renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, drawInfo.renderExtents};
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

    TerrainPushConstants push{4.0f};
    vkCmdPushConstants(cmd, pipelineLayout->layout, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 0, sizeof(TerrainPushConstants), &push);

    constexpr VkDeviceSize zeroOffset{0};

    for (ITerrain* terrain : drawInfo.terrains) {
        terrain::TerrainChunk* terrainChunk = terrain->getTerrainChunk();
        if (!terrainChunk) { continue; }

        std::array descriptorBufferBindingInfo{
            drawInfo.sceneDataBinding,
            terrainChunk->getTextureDescriptorBuffer()->getBindingInfo(),
            terrainChunk->getUniformDescriptorBuffer()->getBindingInfo(),
        };

        vkCmdBindDescriptorBuffersEXT(cmd, descriptorBufferBindingInfo.size(), descriptorBufferBindingInfo.data());

        std::array<uint32_t, 3> indices{0, 1, 2};
        std::array offsets{
            drawInfo.sceneDataOffset,
            ZERO_DEVICE_SIZE,
            drawInfo.currentFrameOverlap * terrainChunk->getUniformDescriptorBuffer()->getDescriptorBufferSize()
        };

        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout->layout, 0, 3, indices.data(), offsets.data());

        VkBuffer vertexBuffer = terrainChunk->getVertexBuffer();
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &zeroOffset);
        vkCmdBindIndexBuffer(cmd, terrainChunk->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, terrainChunk->getIndexCount(), 1, 0, 0, 0);
    }

    vkCmdEndRendering(cmd);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void TerrainPipeline::createPipeline()
{
    resourceManager.destroyResource(std::move(pipeline));
    ShaderModulePtr vertShader = resourceManager.createResource<ShaderModule>("shaders/terrain/terrain.vert");
    ShaderModulePtr tescShader = resourceManager.createResource<ShaderModule>("shaders/terrain/terrain.tesc");
    ShaderModulePtr teseShader = resourceManager.createResource<ShaderModule>("shaders/terrain/terrain.tese");
    ShaderModulePtr fragShader = resourceManager.createResource<ShaderModule>("shaders/terrain/terrain.frag");

    RenderPipelineBuilder renderPipelineBuilder;
    const std::vector<VkVertexInputBindingDescription> vertexBindings{
        {
            .binding = 0,
            .stride = sizeof(terrain::TerrainVertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        }
    };

    const std::vector<VkVertexInputAttributeDescription> vertexAttributes{
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(terrain::TerrainVertex, position),
        },
        {
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(terrain::TerrainVertex, normal),
        },
        {
            .location = 2,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(terrain::TerrainVertex, uv),
        },
        {
            .location = 3,
            .binding = 0,
            .format = VK_FORMAT_R32_SINT,
            .offset = offsetof(terrain::TerrainVertex, materialIndex),
        },
        {
            .location = 4,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = offsetof(terrain::TerrainVertex, color),
        }
    };

    renderPipelineBuilder.setupVertexInput(vertexBindings, vertexAttributes);

    renderPipelineBuilder.setShaders(vertShader->shader, tescShader->shader, teseShader->shader, fragShader->shader);
    renderPipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, false);
    renderPipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    renderPipelineBuilder.disableMultisampling();
    renderPipelineBuilder.disableBlending();
    renderPipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    renderPipelineBuilder.setupRenderer({NORMAL_FORMAT, ALBEDO_FORMAT, PBR_FORMAT, VELOCITY_FORMAT}, DEPTH_STENCIL_FORMAT);
    renderPipelineBuilder.setupPipelineLayout(pipelineLayout->layout);
    renderPipelineBuilder.setupTessellation(4);
    renderPipelineBuilder.addDynamicState(VK_DYNAMIC_STATE_DEPTH_BIAS);
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = renderPipelineBuilder.generatePipelineCreateInfo();
    pipeline = resourceManager.createResource<Pipeline>(pipelineCreateInfo);
}
}
