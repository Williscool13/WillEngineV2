//
// Created by William on 2025-01-22.
//

#include "basic_render_pipeline.h"

#include "volk/volk.h"
#include "src/renderer/renderer_constants.h"
#include "src/renderer/vk_descriptors.h"
#include "src/renderer/vk_helpers.h"
#include "src/renderer/vk_pipelines.h"

namespace basic_render
{
BasicRenderPipeline::BasicRenderPipeline(ResourceManager& resourceManager) : resourceManager(resourceManager)
{
    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    samplerDescriptorLayout = resourceManager.createDescriptorSetLayout(layoutBuilder, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    samplerDescriptorBuffer = resourceManager.createDescriptorBufferSampler(samplerDescriptorLayout, 1);

    VkPushConstantRange renderPushConstantRange{};
    renderPushConstantRange.offset = 0;
    renderPushConstantRange.size = sizeof(RenderPushConstant);
    renderPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo layoutInfo = vk_helpers::pipelineLayoutCreateInfo();
    layoutInfo.setLayoutCount = 2;

    VkDescriptorSetLayout layouts[2]{samplerDescriptorLayout, resourceManager.getSceneDataLayout()};
    layoutInfo.pSetLayouts = layouts;
    layoutInfo.pPushConstantRanges = &renderPushConstantRange;
    layoutInfo.pushConstantRangeCount = 1;

    pipelineLayout = resourceManager.createPipelineLayout(layoutInfo);

    createPipeline();
}

BasicRenderPipeline::~BasicRenderPipeline()
{
    resourceManager.destroyPipeline(pipeline);
    resourceManager.destroyPipelineLayout(pipelineLayout);
    resourceManager.destroyDescriptorSetLayout(samplerDescriptorLayout);
    resourceManager.destroyDescriptorBuffer(samplerDescriptorBuffer);
}

void BasicRenderPipeline::setupDescriptors(const RenderDescriptorInfo& descriptorInfo)
{
    std::vector<will_engine::DescriptorImageData> imageDescriptor;
    imageDescriptor.reserve(1);

    VkDescriptorImageInfo textureImage{};
    textureImage.sampler = descriptorInfo.sampler;
    textureImage.imageView = descriptorInfo.texture;
    textureImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    imageDescriptor.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, textureImage, false});
    resourceManager.setupDescriptorBufferSampler(samplerDescriptorBuffer, imageDescriptor, 0);
}

void BasicRenderPipeline::draw(VkCommandBuffer cmd, const RenderDrawInfo& drawInfo) const
{
    if (drawInfo.drawImage == VK_NULL_HANDLE || drawInfo.depthImage == VK_NULL_HANDLE) { return; }
    constexpr VkClearValue clearValue = {0.0f, 0};
    const VkRenderingAttachmentInfo colorAttachment = vk_helpers::attachmentInfo(drawInfo.drawImage, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const VkRenderingAttachmentInfo depthAttachment = vk_helpers::attachmentInfo(drawInfo.depthImage, &clearValue, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    const VkRenderingInfo renderInfo = vk_helpers::renderingInfo(drawInfo.renderExtent, &colorAttachment, &depthAttachment);

    vkCmdBeginRendering(cmd, &renderInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Dynamic States
    //  Viewport
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<float>(drawInfo.renderExtent.width);
    viewport.height = static_cast<float>(drawInfo.renderExtent.height);
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

    VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[2];
    descriptorBufferBindingInfo[0] = samplerDescriptorBuffer.getDescriptorBufferBindingInfo();
    descriptorBufferBindingInfo[1] = drawInfo.sceneDataBinding;
    vkCmdBindDescriptorBuffersEXT(cmd, 2, descriptorBufferBindingInfo);
    constexpr uint32_t imageBufferIndex = 0;
    constexpr uint32_t sceneDataIndex = 1;
    const VkDeviceSize sceneDataOffset = drawInfo.sceneDataOffset;

    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &imageBufferIndex, &ZERO_DEVICE_SIZE);
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &sceneDataIndex, &sceneDataOffset);

    const float time = static_cast<float>(SDL_GetTicks64()) / 1000.0f;
    RenderPushConstant push{};
    push.currentFrame = drawInfo.currentFrame;
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(RenderPushConstant), &push);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRendering(cmd);
}

void BasicRenderPipeline::createPipeline()
{
    resourceManager.destroyPipeline(pipeline);
    VkShaderModule vertShader = resourceManager.createShaderModule("shaders/basic/vertex.vert");
    VkShaderModule fragShader = resourceManager.createShaderModule("shaders/basic/fragment.frag");

    PipelineBuilder renderPipelineBuilder;
    renderPipelineBuilder.setShaders(vertShader, fragShader);
    renderPipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    renderPipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    renderPipelineBuilder.disableMultisampling();
    renderPipelineBuilder.setupBlending(PipelineBuilder::BlendMode::NO_BLEND);
    renderPipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    renderPipelineBuilder.setupRenderer({DRAW_FORMAT}, DEPTH_FORMAT);
    renderPipelineBuilder.setupPipelineLayout(pipelineLayout);

    pipeline = resourceManager.createRenderPipeline(renderPipelineBuilder);

    resourceManager.destroyShaderModule(vertShader);
    resourceManager.destroyShaderModule(fragShader);
}
}
