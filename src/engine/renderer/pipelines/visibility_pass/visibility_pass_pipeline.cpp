//
// Created by William on 2025-01-26.
//

#include "visibility_pass_pipeline.h"

#include <array>
#include <cmath>

#include "engine/renderer/resource_manager.h"
#include "engine/renderer/vk_helpers.h"
#include "engine/renderer/assets/render_object/render_object.h"

namespace will_engine::renderer
{
VisibilityPassPipeline::VisibilityPassPipeline(ResourceManager& resourceManager)
    : resourceManager(resourceManager)
{
    VkDescriptorSetLayout layouts[3];
    layouts[0] = resourceManager.getSceneDataLayout();
    layouts[1] = resourceManager.getRenderObjectAddressesLayout();
    layouts[2] = resourceManager.getFrustumCullLayout();

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

void VisibilityPassPipeline::draw(VkCommandBuffer cmd, const VisibilityPassDrawInfo& drawInfo) const
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

    for (const RenderObject* renderObject : drawInfo.renderObjects) {
        if (drawInfo.bIsOpaque) {
            if (!renderObject->canDrawOpaque()) { continue; }
        }
        else {
            if (!renderObject->canDrawTransparent()) { continue; }
        }

        std::array bindings{
            drawInfo.sceneDataBinding,
            renderObject->getAddressesDescriptorBuffer().getBindingInfo(),
            renderObject->getFrustumCullingAddressesDescriptorBuffer().getBindingInfo()
        };
        vkCmdBindDescriptorBuffersEXT(cmd, 3, bindings.data());

        std::array offsets{
            drawInfo.sceneDataOffset,
            renderObject->getAddressesDescriptorBuffer().getDescriptorBufferSize() * drawInfo.currentFrameOverlap,
            renderObject->getFrustumCullingAddressesDescriptorBuffer().getDescriptorBufferSize() * drawInfo.currentFrameOverlap
        };

        constexpr std::array<uint32_t, 3> indices{0, 1, 2};

        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout->layout, 0, 3, indices.data(), offsets.data());

        vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(static_cast<float>(renderObject->getOpaqueDrawIndirectCommandCount()) / 64.0f)), 1, 1);

        vk_helpers::uniformBarrier(cmd, renderObject->getOpaqueIndirectBuffer(drawInfo.currentFrameOverlap),
                                       VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                                       VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
    }

    vkCmdEndDebugUtilsLabelEXT(cmd);
}


void VisibilityPassPipeline::createPipeline()
{
    resourceManager.destroyResource(std::move(pipeline));
    VkShaderModule computeShader = resourceManager.createShaderModule("shaders/visibility_pass.comp");

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = computeShader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo;
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.layout = pipelineLayout->layout;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    pipeline = resourceManager.createResource<Pipeline>(pipelineInfo);
    resourceManager.destroyShaderModule(computeShader);
}

}
