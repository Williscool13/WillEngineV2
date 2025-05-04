//
// Created by William on 2025-05-04.
//

#include "debug_highlight_pipeline.h"

#include "src/renderer/assets/render_object/render_object_types.h"


namespace will_engine::debug_highlight_pipeline
{
DebugHighlightPipeline::DebugHighlightPipeline(ResourceManager& resourceManager) : resourceManager(resourceManager)
{
    std::array<VkDescriptorSetLayout, 1> descriptorLayout;
    descriptorLayout[0] = resourceManager.getSceneDataLayout();

    VkPushConstantRange pushConstants{};
    pushConstants.offset = 0;
    pushConstants.size = sizeof(DebugHighlightDrawPushConstant);
    pushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo layoutInfo = vk_helpers::pipelineLayoutCreateInfo();
    layoutInfo.pNext = nullptr;
    layoutInfo.pSetLayouts = descriptorLayout.data();
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstants;
    layoutInfo.pushConstantRangeCount = 1;

    pipelineLayout = resourceManager.createPipelineLayout(layoutInfo);

    //debugHighlightStencil =
}

DebugHighlightPipeline::~DebugHighlightPipeline() {}

void DebugHighlightPipeline::draw(VkCommandBuffer cmd) const
{}

void DebugHighlightPipeline::createPipeline()
{
    VkShaderModule vertShader = resourceManager.createShaderModule("shaders/debug/debug_highlight_renderer.vert");
    VkShaderModule fragShader = resourceManager.createShaderModule("shaders/debug/debug_highlight_renderer.frag");

    PipelineBuilder renderPipelineBuilder;
    VkVertexInputBindingDescription vertexBinding{};
    vertexBinding.binding = 0;
    vertexBinding.stride = sizeof(VertexPosition);
    vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 1> vertexAttributes;
    vertexAttributes[0].binding = 0;
    vertexAttributes[0].location = 0;
    vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexAttributes[0].offset = offsetof(VertexPosition, position);

    renderPipelineBuilder.setupVertexInput(&vertexBinding, 1, vertexAttributes.data(), 1);

    renderPipelineBuilder.setShaders(vertShader, fragShader);
    renderPipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
    renderPipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    renderPipelineBuilder.disableMultisampling();
    renderPipelineBuilder.disableBlending();
    renderPipelineBuilder.disableDepthTest();
    renderPipelineBuilder.setupRenderer({}, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED);
    renderPipelineBuilder.setupPipelineLayout(pipelineLayout);
    const std::vector additionalDynamicStates{VK_DYNAMIC_STATE_LINE_WIDTH};
    pipeline = resourceManager.createRenderPipeline(renderPipelineBuilder, additionalDynamicStates);

    resourceManager.destroyShaderModule(vertShader);
    resourceManager.destroyShaderModule(fragShader);
}
}
