//
// Created by William on 2025-01-22.
//

#include "basic_render_pipeline.h"

#include "engine/renderer/renderer_constants.h"
#include "engine/renderer/resource_manager.h"
#include "engine/renderer/vk_descriptors.h"
#include "engine/renderer/vk_helpers.h"
#include "engine/renderer/vk_pipelines.h"

namespace will_engine::renderer
{
BasicRenderPipeline::BasicRenderPipeline(ResourceManager& resourceManager) : resourceManager(resourceManager)
{
    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    samplerDescriptorLayout = resourceManager.createDescriptorSetLayout(layoutBuilder, VK_SHADER_STAGE_FRAGMENT_BIT,
                                                                        VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    samplerDescriptorBuffer = resourceManager.createDescriptorBufferSampler(samplerDescriptorLayout.layout, 1);

    VkPushConstantRange renderPushConstantRange{};
    renderPushConstantRange.offset = 0;
    renderPushConstantRange.size = sizeof(RenderPushConstant);
    renderPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo layoutInfo = vk_helpers::pipelineLayoutCreateInfo();
    layoutInfo.setLayoutCount = 2;

    VkDescriptorSetLayout layouts[2]{resourceManager.getSceneDataLayout(), samplerDescriptorLayout.layout};
    layoutInfo.pSetLayouts = layouts;
    layoutInfo.pPushConstantRanges = &renderPushConstantRange;
    layoutInfo.pushConstantRangeCount = 1;

    pipelineLayout = resourceManager.createPipelineLayout(layoutInfo);

    createPipeline();
}

BasicRenderPipeline::~BasicRenderPipeline()
{
    resourceManager.destroyResource(std::move(pipeline));
    resourceManager.destroyResource(std::move(pipelineLayout));
    resourceManager.destroyResource(std::move(samplerDescriptorLayout));
    resourceManager.destroyResource(std::move(samplerDescriptorBuffer));
}

void BasicRenderPipeline::setupDescriptors(const RenderDescriptorInfo& descriptorInfo)
{
    std::vector<DescriptorImageData> imageDescriptor;
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
    constexpr VkClearValue colorClear = {.color = {0.0f, 0.0f, 0.0f, 0.0f}};
    constexpr VkClearValue depthClear = {.depthStencil = {0.0f, 0u}};
    const VkRenderingAttachmentInfo colorAttachment = vk_helpers::attachmentInfo(drawInfo.drawImage, &colorClear,
                                                                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const VkRenderingAttachmentInfo depthAttachment = vk_helpers::attachmentInfo(drawInfo.depthImage, &depthClear,
                                                                                 VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    const VkRenderingInfo renderInfo = vk_helpers::renderingInfo(drawInfo.renderExtent, &colorAttachment, &depthAttachment);

    vkCmdBeginRendering(cmd, &renderInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

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
    descriptorBufferBindingInfo[0] = drawInfo.sceneDataBinding;
    descriptorBufferBindingInfo[1] = samplerDescriptorBuffer.getBindingInfo();
    vkCmdBindDescriptorBuffersEXT(cmd, 2, descriptorBufferBindingInfo);
    constexpr std::array indices{0u, 1u};
    const std::array offsets{drawInfo.sceneDataOffset, ZERO_DEVICE_SIZE};

    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.layout, 0, 2, indices.data(), offsets.data());

    RenderPushConstant push{};
    push.currentFrame = drawInfo.currentFrame;
    vkCmdPushConstants(cmd, pipelineLayout.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(RenderPushConstant), &push);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRendering(cmd);
}

void BasicRenderPipeline::createPipeline()
{
    resourceManager.destroyResource(std::move(pipeline));
    VkShaderModule vertShader = resourceManager.createShaderModule("shaders/basic/vertex.vert");
    VkShaderModule fragShader = resourceManager.createShaderModule("shaders/basic/fragment.frag");

    RenderPipelineBuilder renderPipelineBuilder;
    renderPipelineBuilder.setShaders(vertShader, fragShader);
    renderPipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    renderPipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    renderPipelineBuilder.disableMultisampling();
    renderPipelineBuilder.disableBlending();
    renderPipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    renderPipelineBuilder.setupRenderer({DRAW_FORMAT}, DEPTH_STENCIL_FORMAT);
    renderPipelineBuilder.setupPipelineLayout(pipelineLayout.layout);

    pipeline = resourceManager.createRenderPipeline(renderPipelineBuilder);

    resourceManager.destroyShaderModule(vertShader);
    resourceManager.destroyShaderModule(fragShader);
}
}
