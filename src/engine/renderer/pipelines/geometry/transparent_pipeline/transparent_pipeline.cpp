//
// Created by William on 2025-04-06.
//

#include "transparent_pipeline.h"

#include <array>

#include "engine/renderer/renderer_constants.h"
#include "engine/renderer/resource_manager.h"
#include "engine/renderer/vk_descriptors.h"
#include "engine/renderer/vk_helpers.h"
#include "engine/renderer/vk_pipelines.h"
#include "engine/renderer/resources/image.h"
#include "engine/renderer/assets/render_object/render_object.h"
#include "engine/renderer/assets/render_object/render_object_types.h"
#include "engine/renderer/resources/pipeline.h"
#include "engine/renderer/resources/pipeline_layout.h"
#include "engine/renderer/resources/shader_module.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_sampler.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_types.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_uniform.h"

namespace will_engine::renderer
{
TransparentPipeline::TransparentPipeline(ResourceManager& resourceManager,
                                         RenderContext& renderContext,
                                         VkDescriptorSetLayout environmentIBLLayout,
                                         VkDescriptorSetLayout cascadeUniformLayout,
                                         VkDescriptorSetLayout cascadeSamplerLayout): resourceManager(resourceManager)
{
    std::array descriptorLayout{
        resourceManager.getSceneDataLayout(),
        resourceManager.getRenderObjectAddressesLayout(),
        resourceManager.getTexturesLayout(),
        environmentIBLLayout,
        cascadeUniformLayout,
        cascadeSamplerLayout,
    };


    VkPushConstantRange pushConstants = {};
    pushConstants.offset = 0;
    pushConstants.size = sizeof(TransparentPushConstants);
    pushConstants.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;


    VkPipelineLayoutCreateInfo layoutInfo = vk_helpers::pipelineLayoutCreateInfo();
    layoutInfo.pNext = nullptr;
    layoutInfo.pSetLayouts = descriptorLayout.data();
    layoutInfo.setLayoutCount = descriptorLayout.size();
    layoutInfo.pPushConstantRanges = &pushConstants;
    layoutInfo.pushConstantRangeCount = 1;

    accumulationPipelineLayout = resourceManager.createResource<PipelineLayout>(layoutInfo);


    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // Accumulation
    layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // Revealage
    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = layoutBuilder.build(
        VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    compositeDescriptorSetLayout = resourceManager.createResource<DescriptorSetLayout>(layoutCreateInfo);


    const std::array compositeDescriptor{
        compositeDescriptorSetLayout->layout
    };

    VkPipelineLayoutCreateInfo compositeLayoutInfo = vk_helpers::pipelineLayoutCreateInfo();
    compositeLayoutInfo.pNext = nullptr;
    compositeLayoutInfo.pSetLayouts = compositeDescriptor.data();
    compositeLayoutInfo.setLayoutCount = compositeDescriptor.size();
    compositeLayoutInfo.pPushConstantRanges = nullptr;
    compositeLayoutInfo.pushConstantRangeCount = 0;

    compositePipelineLayout = resourceManager.createResource<PipelineLayout>(compositeLayoutInfo);

    compositeDescriptorBuffer = resourceManager.createResource<DescriptorBufferSampler>(compositeDescriptorSetLayout->layout, 1);

    createIntermediateRenderTargets(renderContext.renderExtent);
    createAccumulationPipeline();
    createCompositePipeline();

    resolutionChangedHandle = renderContext.resolutionChangedEvent.subscribe([this](const ResolutionChangedEvent& event) {
        this->handleResize(event);
    });
}

TransparentPipeline::~TransparentPipeline()
{
    resourceManager.destroyResource(std::move(accumulationImage));
    resourceManager.destroyResource(std::move(revealageImage));
    resourceManager.destroyResource(std::move(debugImage));
    resourceManager.destroyResource(std::move(accumulationPipelineLayout));
    resourceManager.destroyResource(std::move(accumulationPipeline));
    resourceManager.destroyResource(std::move(compositeDescriptorSetLayout));
    resourceManager.destroyResource(std::move(compositePipelineLayout));
    resourceManager.destroyResource(std::move(compositePipeline));
    resourceManager.destroyResource(std::move(compositeDescriptorBuffer));
}

void TransparentPipeline::drawAccumulate(VkCommandBuffer cmd, const TransparentAccumulateDrawInfo& drawInfo) const
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Transparent Accumulation Pass (Render Objects)";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    accumulationImage->imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    revealageImage->imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    debugImage->imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vk_helpers::imageBarrier(cmd, accumulationImage.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::imageBarrier(cmd, revealageImage.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::imageBarrier(cmd, debugImage.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);


    constexpr VkClearValue colorClear = {.color = {0.0f, 0.0f, 0.0f, 0.0f}};
    constexpr VkClearValue revealageClear = {.color = {1.0f, 1.0f, 1.0f, 1.0f}};


    VkRenderingAttachmentInfo accumulationAttachment = vk_helpers::attachmentInfo(accumulationImage->imageView, &colorClear,
                                                                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo revealageAttachment = vk_helpers::attachmentInfo(revealageImage->imageView, &revealageClear,
                                                                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo debugAttachment = vk_helpers::attachmentInfo(debugImage->imageView, &colorClear,
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

    renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, drawInfo.renderExtent};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 3;
    renderInfo.pColorAttachments = transparentAttachments;
    renderInfo.pDepthAttachment = &depthAttachment;
    renderInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(cmd, &renderInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, accumulationPipeline->pipeline);

    TransparentPushConstants pushConstants = {};
    pushConstants.bEnabled = drawInfo.enabled;

    vkCmdPushConstants(cmd, accumulationPipelineLayout->layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(TransparentPushConstants), &pushConstants);

    //  Viewport
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = drawInfo.renderExtent.width;
    viewport.height = drawInfo.renderExtent.height;
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

    for (RenderObject* renderObject : drawInfo.renderObjects) {
        if (!renderObject->canDraw()) { continue; }

        std::array descriptorBufferBindingInfos{
            drawInfo.sceneDataBinding,
            renderObject->getAddressesDescriptorBuffer()->getBindingInfo(),
            renderObject->getTextureDescriptorBuffer()->getBindingInfo(),
            drawInfo.environmentIBLBinding,
            drawInfo.cascadeUniformBinding,
            drawInfo.cascadeSamplerBinding,
        };
        vkCmdBindDescriptorBuffersEXT(cmd, 6, descriptorBufferBindingInfos.data());

        constexpr std::array<uint32_t, 6> indices{0, 1, 2, 3, 4, 5};

        std::array offsets{
            drawInfo.sceneDataOffset,
            renderObject->getAddressesDescriptorBuffer()->getDescriptorBufferSize() * drawInfo.currentFrameOverlap,
            ZERO_DEVICE_SIZE,
            drawInfo.environmentIBLOffset,
            drawInfo.cascadeUniformOffset,
            ZERO_DEVICE_SIZE
        };

        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, accumulationPipelineLayout->layout, 0, 6, indices.data(),
                                           offsets.data());

        const VkBuffer vertexBuffers[2] = {renderObject->getPositionVertexBuffer(), renderObject->getPropertyVertexBuffer()};
        constexpr VkDeviceSize vertexOffsets[2] = {0, 0};
        vkCmdBindVertexBuffers(cmd, 0, 2, vertexBuffers, vertexOffsets);
        vkCmdBindIndexBuffer(cmd, renderObject->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexedIndirectCount(cmd,
                              renderObject->getTransparentIndirectBuffer(drawInfo.currentFrameOverlap), 0,
                              renderObject->getDrawCountBuffer(drawInfo.currentFrameOverlap), renderObject->getDrawCountTransparentOffset(),
                              renderObject->getMaxDrawCount(), sizeof(VkDrawIndexedIndirectCommand));
    }

    vkCmdEndRendering(cmd);

    vk_helpers::imageBarrier(cmd, accumulationImage.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::imageBarrier(cmd, revealageImage.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::imageBarrier(cmd, debugImage.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void TransparentPipeline::drawComposite(VkCommandBuffer cmd, const TransparentCompositeDrawInfo& drawInfo) const
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Transparent Composite Pass";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    const VkRenderingAttachmentInfo opaqueAttachment = vk_helpers::attachmentInfo(drawInfo.opaqueImage, nullptr,
                                                                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.pNext = nullptr;

    VkRenderingAttachmentInfo attachments[1];
    attachments[0] = opaqueAttachment;

    renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, drawInfo.renderExtent};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = attachments;
    renderInfo.pDepthAttachment = nullptr;
    renderInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(cmd, &renderInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipeline->pipeline);

    //  Viewport
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = drawInfo.renderExtent.width;
    viewport.height = drawInfo.renderExtent.height;
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


    const std::array descriptorBufferBindingInfos{
        compositeDescriptorBuffer->getBindingInfo()
    };
    vkCmdBindDescriptorBuffersEXT(cmd, 1, descriptorBufferBindingInfos.data());

    constexpr std::array<uint32_t, 1> indices{0};

    const std::array offsets{
        ZERO_DEVICE_SIZE,
    };

    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipelineLayout->layout, 0, 1, indices.data(), offsets.data());

    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRendering(cmd);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void TransparentPipeline::createAccumulationPipeline()
{
    resourceManager.destroyResource(std::move(accumulationPipeline));
    ShaderModulePtr vertShader = resourceManager.createResource<ShaderModule>("shaders/transparent.vert");
    ShaderModulePtr fragShader = resourceManager.createResource<ShaderModule>("shaders/transparent.frag");

    RenderPipelineBuilder pipelineBuilder;
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
        },
    };

    pipelineBuilder.setupVertexInput(vertexBindings, vertexAttributes);

    pipelineBuilder.setShaders(vertShader->shader, fragShader->shader);
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

    blendAttachmentStates[1].blendEnable = VK_TRUE;
    blendAttachmentStates[1].srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendAttachmentStates[1].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    blendAttachmentStates[1].colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachmentStates[1].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendAttachmentStates[1].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachmentStates[1].alphaBlendOp = VK_BLEND_OP_ADD;
    blendAttachmentStates[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT;

    blendAttachmentStates[2].blendEnable = VK_FALSE;
    blendAttachmentStates[2].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    pipelineBuilder.setupBlending(blendAttachmentStates);
    pipelineBuilder.enableDepthTest(false, VK_COMPARE_OP_GREATER);
    pipelineBuilder.setupRenderer({accumulationImageFormat, revealageImageFormat, debugImageFormat}, DEPTH_STENCIL_FORMAT);
    pipelineBuilder.setupPipelineLayout(accumulationPipelineLayout->layout);
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = pipelineBuilder.generatePipelineCreateInfo();
    accumulationPipeline = resourceManager.createResource<Pipeline>(pipelineCreateInfo);
}

void TransparentPipeline::createCompositePipeline()
{
    resourceManager.destroyResource(std::move(compositePipeline));
    ShaderModulePtr vertShader = resourceManager.createResource<ShaderModule>("shaders/transparentComposite.vert");
    ShaderModulePtr fragShader = resourceManager.createResource<ShaderModule>("shaders/transparentComposite.frag");

    RenderPipelineBuilder pipelineBuilder;
    pipelineBuilder.setShaders(vertShader->shader, fragShader->shader);
    pipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.disableMultisampling();


    std::vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates{1};
    blendAttachmentStates[0].blendEnable = VK_TRUE;
    blendAttachmentStates[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachmentStates[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachmentStates[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachmentStates[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachmentStates[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachmentStates[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendAttachmentStates[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    pipelineBuilder.setupBlending(blendAttachmentStates);
    pipelineBuilder.disableDepthTest();
    pipelineBuilder.setupRenderer({DRAW_FORMAT});
    pipelineBuilder.setupPipelineLayout(compositePipelineLayout->layout);
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = pipelineBuilder.generatePipelineCreateInfo();
    compositePipeline = resourceManager.createResource<Pipeline>(pipelineCreateInfo);
}

void TransparentPipeline::createIntermediateRenderTargets(VkExtent2D extents)
{
    VkImageUsageFlags usage{};
    usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    const VkImageCreateInfo imgInfo = vk_helpers::imageCreateInfo(accumulationImageFormat, usage, {extents.width, extents.height, 1});
    const VkImageCreateInfo revealageImgInfo = vk_helpers::imageCreateInfo(revealageImageFormat, usage, {extents.width, extents.height, 1});
    const VkImageCreateInfo debugImgInfo = vk_helpers::imageCreateInfo(debugImageFormat, usage, {extents.width, extents.height, 1});
    accumulationImage = resourceManager.createResource<Image>(imgInfo);
    revealageImage = resourceManager.createResource<Image>(revealageImgInfo);
    debugImage = resourceManager.createResource<Image>(debugImgInfo);

    VkDescriptorImageInfo accumulation{};
    accumulation.sampler = resourceManager.getDefaultSamplerNearest();
    accumulation.imageView = accumulationImage->imageView;
    accumulation.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDescriptorImageInfo revealage{};
    revealage.sampler = resourceManager.getDefaultSamplerNearest();
    revealage.imageView = revealageImage->imageView;
    revealage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    std::array<DescriptorImageData, 2> descriptors{
        DescriptorImageData{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, accumulation, false},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, revealage, false},
    };

    compositeDescriptorBuffer->setupData(descriptors, 0);
}

void TransparentPipeline::handleResize(const ResolutionChangedEvent& event)
{
    resourceManager.destroyResourceImmediate(std::move(accumulationImage));
    resourceManager.destroyResourceImmediate(std::move(revealageImage));
    resourceManager.destroyResourceImmediate(std::move(debugImage));

    createIntermediateRenderTargets(event.newExtent);
}
}
