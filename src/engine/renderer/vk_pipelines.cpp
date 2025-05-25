//
// Created by William on 2024-08-18.
//

#include "vk_pipelines.h"

#include <volk/volk.h>

namespace will_engine::renderer
{
RenderPipelineBuilder::RenderPipelineBuilder()
{
    clear();
}

VkGraphicsPipelineCreateInfo RenderPipelineBuilder::generatePipelineCreateInfo(VkPipelineCreateFlagBits flags)
{
    if (bBlendingDisabled) {
        constexpr VkPipelineColorBlendAttachmentState colorBlendAttachment{
            VK_FALSE,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        };

        blendAttachmentStates.clear();
        for (int i = 0; i < colorAttachmentFormats.size(); i++) {
            blendAttachmentStates.push_back(colorBlendAttachment);
        }
    }

    assert(colorAttachmentFormats.size() == blendAttachmentStates.size());
    colorBlending.pAttachments = blendAttachmentStates.data();
    colorBlending.attachmentCount = blendAttachmentStates.size();

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pNext = &renderInfo; // for pipeline creation w/ dynamic rendering
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    if (bIsTessellationEnabled) {
        assert(shaderStages.size() > 3);
        pipelineInfo.pTessellationState = &tessellation;
    }
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.flags = flags;
    pipelineInfo.pDynamicState = &dynamicInfo;

    return pipelineInfo;
}

void RenderPipelineBuilder::clear()
{
    shaderStages.clear();
    inputAssembly = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    rasterizer = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    multisampling = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    pipelineLayout = {};
    depthStencil = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    renderInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
}

void RenderPipelineBuilder::setShaders(const VkShaderModule vertexShader)
{
    shaderStages.clear();
    shaderStages.push_back(
        vk_helpers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader)
    );
}

void RenderPipelineBuilder::setShaders(const VkShaderModule vertexShader, const VkShaderModule fragmentShader)
{
    shaderStages.clear();
    shaderStages.push_back(
        vk_helpers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader)
    );

    shaderStages.push_back(
        vk_helpers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader)
    );
}

void RenderPipelineBuilder::setShaders(const VkShaderModule vertexShader, const VkShaderModule tessControlShader, const VkShaderModule tessEvalShader,
                                       const VkShaderModule fragmentShader)
{
    shaderStages.clear();
    shaderStages.push_back(
        vk_helpers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader)
    );

    shaderStages.push_back(
        vk_helpers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, tessControlShader)
    );

    shaderStages.push_back(
        vk_helpers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, tessEvalShader)
    );

    shaderStages.push_back(
        vk_helpers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader)
    );
}

void RenderPipelineBuilder::setupVertexInput(const std::vector<VkVertexInputBindingDescription>& bindings,
                                             const std::vector<VkVertexInputAttributeDescription>& attributes)
{
    this->vertexBindings = bindings;
    this->vertexAttributes = attributes;

    if (vertexAttributes.size() > 0) {
        vertexInputInfo.pVertexAttributeDescriptions = vertexAttributes.data();
        vertexInputInfo.vertexAttributeDescriptionCount = vertexAttributes.size();
    }
    if (vertexBindings.size() > 0) {
        vertexInputInfo.pVertexBindingDescriptions = vertexBindings.data();
        vertexInputInfo.vertexBindingDescriptionCount = vertexBindings.size();
    }
}

void RenderPipelineBuilder::setupInputAssembly(const VkPrimitiveTopology topology, const bool enablePrimitiveRestart)
{
    inputAssembly.topology = topology;
    inputAssembly.primitiveRestartEnable = enablePrimitiveRestart;
}

void RenderPipelineBuilder::setupRasterization(const VkPolygonMode polygonMode, const VkCullModeFlags cullMode, const VkFrontFace frontFace,
                                               const float lineWidth, const bool rasterizerDiscardEnable)
{
    // Draw Mode
    rasterizer.polygonMode = polygonMode;
    rasterizer.lineWidth = lineWidth;

    // Culling
    rasterizer.cullMode = cullMode;
    rasterizer.frontFace = frontFace;

    rasterizer.rasterizerDiscardEnable = rasterizerDiscardEnable;
}

void RenderPipelineBuilder::enableDepthBias(const float depthBiasConstantFactor, const float depthBiasClamp, const float depthBiasSlopeFactor)
{
    rasterizer.depthBiasEnable = true;
    rasterizer.depthBiasConstantFactor = depthBiasConstantFactor;
    rasterizer.depthBiasClamp = depthBiasClamp;
    rasterizer.depthBiasSlopeFactor = depthBiasSlopeFactor;
}

void RenderPipelineBuilder::setupMultisampling(const VkBool32 sampleShadingEnable, const VkSampleCountFlagBits rasterizationSamples,
                                               const float minSampleShading, const VkSampleMask* pSampleMask, const VkBool32 alphaToCoverageEnable,
                                               const VkBool32 alphaToOneEnable)
{
    multisampling.sampleShadingEnable = sampleShadingEnable;

    multisampling.rasterizationSamples = rasterizationSamples;
    multisampling.minSampleShading = minSampleShading;
    multisampling.pSampleMask = pSampleMask;
    // A2C
    multisampling.alphaToCoverageEnable = alphaToCoverageEnable;
    multisampling.alphaToOneEnable = alphaToOneEnable;
}

void RenderPipelineBuilder::disableMultisampling()
{
    setupMultisampling(VK_FALSE, VK_SAMPLE_COUNT_1_BIT, 1.0f, nullptr, VK_FALSE, VK_FALSE);
}

void RenderPipelineBuilder::setupRenderer(const std::vector<VkFormat>& colorAttachmentFormat, const VkFormat depthAttachmentFormat,
                                          const VkFormat stencilAttachmentFormat)
{
    // Color Format
    if (!colorAttachmentFormat.empty()) {
        colorAttachmentFormats = colorAttachmentFormat;
        renderInfo.colorAttachmentCount = colorAttachmentFormats.size();
        renderInfo.pColorAttachmentFormats = colorAttachmentFormats.data();
    }

    // Depth Format
    if (depthAttachmentFormat != VK_FORMAT_UNDEFINED) {
        renderInfo.depthAttachmentFormat = depthAttachmentFormat;
    }

    // Stencil Format
    if (stencilAttachmentFormat != VK_FORMAT_UNDEFINED) {
        renderInfo.stencilAttachmentFormat = stencilAttachmentFormat;
    }
}

void RenderPipelineBuilder::setupDepthStencil(const VkBool32 depthTestEnable, const VkBool32 depthWriteEnable, const VkCompareOp compareOp,
                                              const VkBool32 depthBoundsTestEnable, const VkBool32 stencilTestEnable, const VkStencilOpState& front,
                                              const VkStencilOpState& back, const float minDepthBounds, const float maxDepthBounds)
{
    depthStencil.depthTestEnable = depthTestEnable;
    depthStencil.depthWriteEnable = depthWriteEnable;
    depthStencil.depthCompareOp = compareOp;
    depthStencil.depthBoundsTestEnable = depthBoundsTestEnable;
    depthStencil.stencilTestEnable = stencilTestEnable;
    depthStencil.front = front;
    depthStencil.back = back;
    depthStencil.minDepthBounds = minDepthBounds;
    depthStencil.maxDepthBounds = maxDepthBounds;
}

void RenderPipelineBuilder::enableDepthTest(const VkBool32 depthWriteEnable, const VkCompareOp op)
{
    setupDepthStencil(
        VK_TRUE, depthWriteEnable, op,
        VK_FALSE, VK_FALSE, {}, {}, 0.0f, 1.0f
    );
}

void RenderPipelineBuilder::disableDepthTest()
{
    setupDepthStencil(
        VK_FALSE, VK_FALSE, VK_COMPARE_OP_NEVER,
        VK_FALSE, VK_FALSE, {}, {}, 0.0f, 1.0f
    );
}

void RenderPipelineBuilder::setupBlending(const std::vector<VkPipelineColorBlendAttachmentState>& blendAttachmentStates)
{
    bBlendingDisabled = false;
    this->blendAttachmentStates = blendAttachmentStates;
}

void RenderPipelineBuilder::disableBlending()
{
    bBlendingDisabled = true;
}

void RenderPipelineBuilder::setupPipelineLayout(const VkPipelineLayout pipelineLayout)
{
    this->pipelineLayout = pipelineLayout;
}

void RenderPipelineBuilder::setupTessellation(const int32_t controlPoints)
{
    bIsTessellationEnabled = true;
    tessellation.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    tessellation.patchControlPoints = controlPoints;
}

void RenderPipelineBuilder::addDynamicState(const VkDynamicState dynamicState)
{
    dynamicStates.push_back(dynamicState);
    dynamicInfo.pDynamicStates = dynamicStates.data();
    dynamicInfo.dynamicStateCount = dynamicStates.size();
}
}
