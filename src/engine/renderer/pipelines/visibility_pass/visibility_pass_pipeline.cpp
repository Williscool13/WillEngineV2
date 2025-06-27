//
// Created by William on 2025-01-26.
//

#include "visibility_pass_pipeline.h"

#include <array>
#include <cmath>

#include "engine/renderer/resource_manager.h"
#include "engine/renderer/vk_helpers.h"
#include "engine/renderer/assets/render_object/render_object.h"
#include "engine/renderer/resources/pipeline.h"
#include "engine/renderer/resources/pipeline_layout.h"
#include "engine/renderer/resources/shader_module.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_uniform.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_sampler.h"

namespace will_engine::renderer
{
VisibilityPassPipeline::VisibilityPassPipeline(ResourceManager& resourceManager)
    : resourceManager(resourceManager)
{
    VkDescriptorSetLayout layouts[3];
    layouts[0] = resourceManager.getSceneDataLayout();
    layouts[1] = resourceManager.getRenderObjectAddressesLayout();
    layouts[2] = resourceManager.getVisibilityPassLayout();

    VkPushConstantRange pushConstantRange;
    pushConstantRange.size = sizeof(VisibilityPassPushConstants);
    pushConstantRange.offset = 0;
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 3;
    layoutInfo.pSetLayouts = layouts;
    layoutInfo.pPushConstantRanges = &pushConstantRange;
    layoutInfo.pushConstantRangeCount = 1;
    pipelineLayout = resourceManager.createResource<PipelineLayout>(layoutInfo);


    createPipeline();
}

VisibilityPassPipeline::~VisibilityPassPipeline()
{
    resourceManager.destroyResource(std::move(pipeline));
    resourceManager.destroyResource(std::move(pipelineLayout));
}

void VisibilityPassPipeline::draw(VkCommandBuffer cmd, const VisibilityPassDrawInfo& drawInfo)
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Frustum Culling";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline);


    VisibilityPassPushConstants pushConstants = {};
    pushConstants.enable = drawInfo.bEnableFrustumCulling;
    pushConstants.shadowPass = drawInfo.bIsShadowPass;

    vkCmdPushConstants(cmd, pipelineLayout->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(VisibilityPassPushConstants), &pushConstants);

    barrierCache.clear();
    barrierCache.reserve(drawInfo.renderObjects.size() * 3);

    for (const RenderObject* renderObject : drawInfo.renderObjects) {
        if (!renderObject->canDraw()) {
            continue;
        }

        const VkBuffer drawCountBuffer = renderObject->getDrawCountBuffer(drawInfo.currentFrameOverlap);
        vkCmdFillBuffer(cmd, drawCountBuffer,offsetof(IndirectCount, opaqueCount), sizeof(uint32_t) * 2, 0);
        const uint32_t limit = renderObject->getMaxDrawCount();
        vkCmdFillBuffer(cmd, drawCountBuffer,offsetof(IndirectCount, limit), sizeof(uint32_t), limit);
        vk_helpers::bufferBarrier(cmd, drawCountBuffer, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                  VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                  VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);



        std::array bindings{
            drawInfo.sceneDataBinding,
            renderObject->getVisibilityPassDescriptorBuffer()->getBindingInfo()
        };
        vkCmdBindDescriptorBuffersEXT(cmd, 2, bindings.data());

        std::array offsets{
            drawInfo.sceneDataOffset,
            renderObject->getVisibilityPassDescriptorBuffer()->getDescriptorBufferSize() * drawInfo.currentFrameOverlap
        };

        constexpr std::array<uint32_t, 2> indices{0, 1};

        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout->layout, 0, 2, indices.data(), offsets.data());

        const uint32_t dispatchSize = (renderObject->getMaxDrawCount() + 63) / 64;
        vkCmdDispatch(cmd, dispatchSize, 1, 1);

        barrierCache.insert(barrierCache.end(), {
                                {
                                    renderObject->getOpaqueIndirectBuffer(drawInfo.currentFrameOverlap),
                                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT
                                },
                                {
                                    renderObject->getTransparentIndirectBuffer(drawInfo.currentFrameOverlap),
                                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT
                                },
                                {
                                    renderObject->getDrawCountBuffer(drawInfo.currentFrameOverlap),
                                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT
                                }
                            });
    }

    if (!barrierCache.empty()) {
        vk_helpers::bufferBarriers(cmd, barrierCache);
    }

    vkCmdEndDebugUtilsLabelEXT(cmd);
}


void VisibilityPassPipeline::createPipeline()
{
    resourceManager.destroyResource(std::move(pipeline));
    ShaderModulePtr shader = resourceManager.createResource<ShaderModule>("shaders/visibility_pass.comp");

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = shader->shader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo;
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.layout = pipelineLayout->layout;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    pipeline = resourceManager.createResource<Pipeline>(pipelineInfo);
}
}
