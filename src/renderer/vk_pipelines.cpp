//
// Created by William on 2024-08-18.
//

#include "vk_pipelines.h"

#include <volk/volk.h>

void will_engine::PipelineBuilder::clear()
{
    vertexInputInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    inputAssembly = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    rasterizer = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    multisampling = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    pipelineLayout = {};
    depthStencil = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    renderInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    shaderStages.clear();
}

VkPipeline will_engine::PipelineBuilder::buildPipeline(VkDevice device, VkPipelineCreateFlagBits flags, std::vector<VkDynamicState> additionalDynamicStates)
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
    if (bBlendingDisabled) {
        // all disabled
        VkPipelineColorBlendAttachmentState colorBlendAttachment{
            VK_FALSE,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        };

        for (int i = 0; i < colorAttachmentFormats.size(); i++) {
            blendAttachments[i] = colorBlendAttachment;
        }
        colorBlending.pAttachments = blendAttachments;
    } else {
        // must have same number of color blend attachments as color attachments
        assert(colorAttachmentFormats.size() == blendAttachmentStates.size());

        colorBlending.pAttachments = blendAttachmentStates.data();
    }


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

    // Dynamic state
    additionalDynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    additionalDynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
    const VkPipelineDynamicStateCreateInfo dynamicInfo = generateDynamicStates(additionalDynamicStates.data(), additionalDynamicStates.size());
    pipelineInfo.pDynamicState = &dynamicInfo;

    // Create Pipeline
    VkPipeline newPipeline;
    const VkResult response = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline);
    delete blendAttachments;

    if (response != VK_SUCCESS) {
        fmt::print("failed to create pipeline");
        return VK_NULL_HANDLE;
    }
    else {
        return newPipeline;
    }
}

void will_engine::PipelineBuilder::setShaders(VkShaderModule vertexShader)
{
    shaderStages.clear();

    shaderStages.push_back(
        vk_helpers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader)
    );
}

void will_engine::PipelineBuilder::setShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader)
{
    shaderStages.clear();

    shaderStages.push_back(
        vk_helpers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader)
    );

    shaderStages.push_back(
        vk_helpers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader)
    );
}

void will_engine::PipelineBuilder::setShaders(const VkShaderModule vertexShader, const VkShaderModule tessControlShader, const VkShaderModule tessEvalShader, const VkShaderModule fragmentShader)
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

void will_engine::PipelineBuilder::setupVertexInput(const VkVertexInputBindingDescription* bindings, const uint32_t bindingCount, const VkVertexInputAttributeDescription* attributes, const uint32_t attributeCount)
{
    vertexInputEnabled = true;
    vertexInputInfo.pVertexBindingDescriptions = bindings;
    vertexInputInfo.vertexBindingDescriptionCount = bindingCount;
    vertexInputInfo.pVertexAttributeDescriptions = attributes;
    vertexInputInfo.vertexAttributeDescriptionCount = attributeCount;
}

void will_engine::PipelineBuilder::setupInputAssembly(const VkPrimitiveTopology topology, const bool enablePrimitiveRestart)
{
    inputAssembly.topology = topology;
    inputAssembly.primitiveRestartEnable = enablePrimitiveRestart;
}

void will_engine::PipelineBuilder::setupRasterization(VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace,
                                                      float lineWidth, bool rasterizerDiscardEnable)
{
    // Draw Mode
    rasterizer.polygonMode = polygonMode;
    rasterizer.lineWidth = lineWidth;

    // Culling
    rasterizer.cullMode = cullMode;
    rasterizer.frontFace = frontFace;

    rasterizer.rasterizerDiscardEnable = rasterizerDiscardEnable;
}

void will_engine::PipelineBuilder::enableDepthBias(const float depthBiasConstantFactor, const float depthBiasClamp, const float depthBiasSlopeFactor)
{
    rasterizer.depthBiasEnable = true;
    rasterizer.depthBiasConstantFactor = depthBiasConstantFactor;
    rasterizer.depthBiasClamp = depthBiasClamp;
    rasterizer.depthBiasSlopeFactor = depthBiasSlopeFactor;
}

void will_engine::PipelineBuilder::setupMultisampling(VkBool32 sampleShadingEnable, VkSampleCountFlagBits rasterizationSamples, float minSampleShading,
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

void will_engine::PipelineBuilder::setupRenderer(const std::vector<VkFormat>& colorAttachmentFormat, const VkFormat depthAttachmentFormat, const VkFormat stencilAttachmentFormat)
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

void will_engine::PipelineBuilder::setupDepthStencil(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp compareOp, VkBool32 depthBoundsTestEnable,
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

void will_engine::PipelineBuilder::disableBlending()
{
    bBlendingDisabled = true;
}

void will_engine::PipelineBuilder::setupBlending(const std::vector<VkPipelineColorBlendAttachmentState>& blendAttachmentStates)
{
    bBlendingDisabled = false;
    this->blendAttachmentStates = blendAttachmentStates;
}

void will_engine::PipelineBuilder::setupPipelineLayout(VkPipelineLayout pipelineLayout)
{
    this->pipelineLayout = pipelineLayout;
}

void will_engine::PipelineBuilder::disableMultisampling()
{
    setupMultisampling(VK_FALSE, VK_SAMPLE_COUNT_1_BIT, 1.0f, nullptr, VK_FALSE, VK_FALSE);
}

void will_engine::PipelineBuilder::enableDepthTest(bool depthWriteEnable, VkCompareOp op)
{
    setupDepthStencil(
        VK_TRUE, depthWriteEnable, op,
        VK_FALSE, VK_FALSE, {}, {}, 0.0f, 1.0f
    );
}

void will_engine::PipelineBuilder::disableDepthTest()
{
    setupDepthStencil(
        VK_FALSE, VK_FALSE, VK_COMPARE_OP_NEVER,
        VK_FALSE, VK_FALSE, {}, {}, 0.0f, 1.0f
    );
}

VkPipelineDynamicStateCreateInfo will_engine::PipelineBuilder::generateDynamicStates(VkDynamicState states[], uint32_t count)
{
    VkPipelineDynamicStateCreateInfo dynamicInfo = {};
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.pDynamicStates = states;
    dynamicInfo.dynamicStateCount = count;
    return dynamicInfo;
}

void will_engine::PipelineBuilder::setupTessellation(const int32_t controlPoints)
{
    bIsTessellationEnabled = true;
    tessellation.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    tessellation.patchControlPoints = controlPoints;
}
