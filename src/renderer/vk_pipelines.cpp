//
// Created by William on 2024-08-18.
//

#include "vk_pipelines.h"

#include <volk.h>

void PipelineBuilder::clear()
{
    vertexInputInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    inputAssembly = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    rasterizer = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    colorBlendAttachment = {};
    multisampling = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    pipelineLayout = {};
    depthStencil = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    renderInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    shaderStages.clear();
}

VkPipeline PipelineBuilder::buildPipeline(VkDevice device, VkPipelineCreateFlagBits flags)
{
    // Viewport, details not necessary here (dynamic rendering)
    VkPipelineViewportStateCreateInfo viewportState = {}; {
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.pNext = nullptr;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
    }

    // Color Blending
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.pNext = nullptr;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = colorAttachmentFormats.size();
    auto* blendAttachments = new VkPipelineColorBlendAttachmentState[colorAttachmentFormats.size()];
    for (int i = 0; i < colorAttachmentFormats.size(); i++) {
        blendAttachments[i] = colorBlendAttachment;
    }
    colorBlending.pAttachments = blendAttachments;

    // (Not used in this codebase) Vertex Attribute Input
    VkPipelineVertexInputStateCreateInfo _vertexInputInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};


    // Build Pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderInfo; // for pipeline creation w/ dynamic rendering

    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pVertexInputState = &_vertexInputInfo;

    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    if (vertexInputEnabled) {
        pipelineInfo.pVertexInputState = &vertexInputInfo;
    }
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout;

    pipelineInfo.flags = flags;

    // Dynamic state
    VkDynamicState state[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const VkPipelineDynamicStateCreateInfo dynamicInfo = generateDynamicStates(state, 2);
    pipelineInfo.pDynamicState = &dynamicInfo;

    // Create Pipeline
    VkPipeline newPipeline;
    const VkResult response = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline);
    delete blendAttachments;

    if (response != VK_SUCCESS) {
        fmt::print("failed to create pipeline");
        return VK_NULL_HANDLE;
    } else {
        return newPipeline;
    }
}

void PipelineBuilder::setShaders(VkShaderModule vertexShader)
{
    shaderStages.clear();

    shaderStages.push_back(
        vk_helpers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader)
    );
}

void PipelineBuilder::setShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader)
{
    shaderStages.clear();

    shaderStages.push_back(
        vk_helpers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader)
    );

    shaderStages.push_back(
        vk_helpers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader)
    );
}

void PipelineBuilder::setupVertexInput(VkVertexInputBindingDescription* bindings, uint32_t bindingCount,
                                       VkVertexInputAttributeDescription* attributes,
                                       uint32_t attributeCount)
{
    vertexInputEnabled = true;
    vertexInputInfo.pVertexBindingDescriptions = bindings;
    vertexInputInfo.vertexBindingDescriptionCount = bindingCount;
    vertexInputInfo.pVertexAttributeDescriptions = attributes;
    vertexInputInfo.vertexAttributeDescriptionCount = attributeCount;
}

void PipelineBuilder::setupInputAssembly(VkPrimitiveTopology topology)
{
    inputAssembly.topology = topology;
    inputAssembly.primitiveRestartEnable = VK_FALSE;
}

void PipelineBuilder::setupRasterization(const VkPolygonMode polygonMode, const VkCullModeFlags cullMode, const VkFrontFace frontFace,
                                         bool rasterizerDiscardEnable)
{
    // Draw Mode
    rasterizer.polygonMode = polygonMode;
    rasterizer.lineWidth = 1.0f;

    // Culling
    rasterizer.cullMode = cullMode;
    rasterizer.frontFace = frontFace;

    rasterizer.rasterizerDiscardEnable = rasterizerDiscardEnable;
}

void PipelineBuilder::setupMultisampling(VkBool32 sampleShadingEnable, VkSampleCountFlagBits rasterizationSamples, float minSampleShading,
                                         const VkSampleMask* pSampleMask, VkBool32 alphaToCoverageEnable, VkBool32 alphaToOneEnable)
{
    multisampling.sampleShadingEnable = sampleShadingEnable;

    multisampling.rasterizationSamples = rasterizationSamples;
    multisampling.minSampleShading = minSampleShading;
    multisampling.pSampleMask = pSampleMask;
    // A2C
    multisampling.alphaToCoverageEnable = alphaToCoverageEnable;
    multisampling.alphaToOneEnable = alphaToOneEnable;
}

void PipelineBuilder::setupRenderer(const std::vector<VkFormat> colorattachmentFormat, const VkFormat depthAttachmentFormat)
{
    // Color Format
    if (!colorattachmentFormat.empty()) {
        colorAttachmentFormats = colorattachmentFormat;
        renderInfo.colorAttachmentCount = colorAttachmentFormats.size();
        renderInfo.pColorAttachmentFormats = colorAttachmentFormats.data();
    }

    // Depth Format
    if (depthAttachmentFormat != VK_FORMAT_UNDEFINED) {
        renderInfo.depthAttachmentFormat = depthAttachmentFormat;
    }
}

void PipelineBuilder::setupDepthStencil(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp compareOp, VkBool32 depthBoundsTestEnable,
                                        VkBool32 stencilTestEnable, VkStencilOpState front, VkStencilOpState back, float minDepthBounds,
                                        float maxDepthBounds)
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

void PipelineBuilder::setupBlending(PipelineBuilder::BlendMode mode)
{
    switch (mode) {
        case BlendMode::ALPHA_BLEND: {
            colorBlendAttachment.colorWriteMask =
                    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachment.blendEnable = VK_TRUE;
            colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
            colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
            colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
            break;
        }
        case BlendMode::ADDITIVE_BLEND: {
            colorBlendAttachment.colorWriteMask =
                    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachment.blendEnable = VK_TRUE;
            colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
            colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
            break;
        }
        case BlendMode::NO_BLEND:
            colorBlendAttachment.colorWriteMask =
                    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachment.blendEnable = VK_FALSE;
            break;
    }
}

void PipelineBuilder::setupPipelineLayout(VkPipelineLayout pipelineLayout)
{
    this->pipelineLayout = pipelineLayout;
}

void PipelineBuilder::disableMultisampling()
{
    setupMultisampling(VK_FALSE, VK_SAMPLE_COUNT_1_BIT, 1.0f, nullptr, VK_FALSE, VK_FALSE);
}

void PipelineBuilder::enableDepthtest(bool depthWriteEnable, VkCompareOp op)
{
    setupDepthStencil(
        VK_TRUE, depthWriteEnable, op,
        VK_FALSE, VK_FALSE, {}, {}, 0.0f, 1.0f
    );
}

void PipelineBuilder::disableDepthtest()
{
    setupDepthStencil(
        VK_FALSE, VK_FALSE, VK_COMPARE_OP_NEVER,
        VK_FALSE, VK_FALSE, {}, {}, 0.0f, 1.0f
    );
}

VkPipelineDynamicStateCreateInfo PipelineBuilder::generateDynamicStates(VkDynamicState states[], uint32_t count)
{
    VkPipelineDynamicStateCreateInfo dynamicInfo = {};
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.pDynamicStates = states;
    dynamicInfo.dynamicStateCount = count;
    return dynamicInfo;
}
