//
// Created by William on 2025-04-27.
//

#include "debug_renderer.h"

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <volk/volk.h>

#include "src/renderer/resource_manager.h"


namespace will_engine::debug_renderer
{
DebugRenderer* DebugRenderer::debugRenderer = nullptr;

DebugRenderer::DebugRenderer(ResourceManager& resourceManager) : resourceManager(resourceManager)
{
    boxInstances.reserve(DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT);
    // 8 vertices, 12 edges (24 points)
    constexpr int32_t boxVertexCount = 8;
    constexpr int32_t boxIndicesCount = 24;
    instancedVertices.reserve(instancedVertices.size() + boxVertexCount);
    instancedIndices.reserve(instancedIndices.size() + boxIndicesCount);

    std::vector<DebugRendererVertex> boxVertices = {
        {{0, 0, 0}}, // 0: near bottom left
        {{1, 0, 0}}, // 1: near bottom right
        {{1, 1, 0}}, // 2: near top right
        {{0, 1, 0}}, // 3: near top left
        {{0, 0, 1}}, // 4: far bottom left
        {{1, 0, 1}}, // 5: far bottom right
        {{1, 1, 1}}, // 6: far top right
        {{0, 1, 1}} // 7: far top left
    };

    std::vector<uint32_t> boxIndices = {
        // Near face
        0, 1, 1, 2, 2, 3, 3, 0,
        // Far face
        4, 5, 5, 6, 6, 7, 7, 4,
        // Connecting edges
        0, 4, 1, 5, 2, 6, 3, 7
    };

    size_t boxVertexOffset = instancedVertices.size();
    size_t boxIndexOffset = instancedIndices.size();
    instancedVertices.insert(instancedVertices.end(), boxVertices.begin(), boxVertices.end());
    instancedIndices.insert(instancedIndices.end(), boxIndices.begin(), boxIndices.end());

    boxDrawIndexedData.indexCount = boxIndicesCount;
    boxDrawIndexedData.firstIndex = boxIndexOffset;
    boxDrawIndexedData.vertexOffset = static_cast<int32_t>(boxVertexOffset);
    boxDrawIndexedData.firstInstance = 0;

    // Vertex Buffer
    const uint64_t instancedVertexBufferSize = instancedVertices.size() * sizeof(DebugRendererVertex);
    const AllocatedBuffer instancedVertexStaging = resourceManager.createStagingBuffer(instancedVertexBufferSize);
    memcpy(instancedVertexStaging.info.pMappedData, instancedVertices.data(), instancedVertexBufferSize);
    instancedVertexBuffer = resourceManager.createDeviceBuffer(instancedVertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    // Index Buffer
    const uint64_t instancedIndexBufferSize = instancedIndices.size() * sizeof(uint32_t);
    const AllocatedBuffer instancedIndexStaging = resourceManager.createStagingBuffer(instancedIndexBufferSize);
    memcpy(instancedIndexStaging.info.pMappedData, instancedIndices.data(), instancedIndexBufferSize);
    instancedIndexBuffer = resourceManager.createDeviceBuffer(instancedIndexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    std::array<BufferCopyInfo, 2> bufferCopies = {
        BufferCopyInfo(instancedVertexStaging, 0, instancedVertexBuffer, 0, instancedVertexBufferSize),
        {instancedIndexStaging, 0, instancedIndexBuffer, 0, instancedIndexBufferSize},
    };

    resourceManager.copyBufferImmediate(bufferCopies);
    for (BufferCopyInfo bufferCopy : bufferCopies) {
        resourceManager.destroyBufferImmediate(bufferCopy.src);
    }

    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    uniformLayout = resourceManager.createDescriptorSetLayout(layoutBuilder, VK_SHADER_STAGE_VERTEX_BIT,
                                                              VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    // Box Instance Data Buffer
    constexpr uint64_t boxInstanceBufferSize = DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT * sizeof(BoxInstance);
    boxInstanceDescriptorBuffer = resourceManager.createDescriptorBufferUniform(uniformLayout, FRAME_OVERLAP);
    for (int32_t i = 0; i < FRAME_OVERLAP; ++i) {
        boxInstanceBuffers[i] = resourceManager.createHostSequentialBuffer(boxInstanceBufferSize);
        DescriptorUniformData addressesUniformData{
            .uniformBuffer = boxInstanceBuffers[i],
            .allocSize = boxInstanceBufferSize,
        };

        resourceManager.setupDescriptorBufferUniform(boxInstanceDescriptorBuffer, {addressesUniformData}, i);
    }

    VkDescriptorSetLayout descriptorLayout[2];
    descriptorLayout[0] = resourceManager.getSceneDataLayout();
    descriptorLayout[1] = uniformLayout;


    VkPipelineLayoutCreateInfo layoutInfo = vk_helpers::pipelineLayoutCreateInfo();
    layoutInfo.pSetLayouts = descriptorLayout;
    layoutInfo.pNext = nullptr;
    layoutInfo.setLayoutCount = 2;
    layoutInfo.pPushConstantRanges = nullptr;
    layoutInfo.pushConstantRangeCount = 0;

    pipelineLayout = resourceManager.createPipelineLayout(layoutInfo);

    createPipeline();
}

DebugRenderer::~DebugRenderer()
{
    resourceManager.destroyDescriptorSetLayout(uniformLayout);

    resourceManager.destroyBuffer(instancedVertexBuffer);
    resourceManager.destroyBuffer(instancedIndexBuffer);

    for (AllocatedBuffer& buffer : boxInstanceBuffers) {
        resourceManager.destroyBuffer(buffer);
    }
    resourceManager.destroyDescriptorBuffer(boxInstanceDescriptorBuffer);

    resourceManager.destroyPipeline(pipeline);
    resourceManager.destroyPipelineLayout(pipelineLayout);
}

void DebugRenderer::draw(VkCommandBuffer cmd, const DebugRendererDrawInfo& drawInfo)
{
    if (boxInstances.empty()) { return; }

    if (boxInstances.size() > DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT) {
        boxInstances.resize(DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT);
    }

    // upload
    const AllocatedBuffer& currentBoxInstanceBuffer = boxInstanceBuffers[drawInfo.currentFrameOverlap];
    memcpy(currentBoxInstanceBuffer.info.pMappedData, boxInstances.data(), sizeof(BoxInstance) * boxInstances.size());

    const VkRenderingAttachmentInfo albedoAttachment = vk_helpers::attachmentInfo(drawInfo.albedoTarget, nullptr,
                                                                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    const VkRenderingAttachmentInfo depthAttachment = vk_helpers::attachmentInfo(drawInfo.depthTarget, nullptr,
                                                                                 VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.pNext = nullptr;

    VkRenderingAttachmentInfo renderAttachments[1];
    renderAttachments[0] = albedoAttachment;

    renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, RENDER_EXTENTS};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = renderAttachments;
    renderInfo.pDepthAttachment = &depthAttachment;
    renderInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(cmd, &renderInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    vkCmdSetLineWidth(cmd, 3.0f);

    //  Viewport
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = RENDER_EXTENTS.width;
    viewport.height = RENDER_EXTENTS.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    //  Scissor
    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = RENDER_EXTENTS.width;
    scissor.extent.height = RENDER_EXTENTS.height;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    const std::array descriptorBufferBindingInfos{
        drawInfo.sceneDataBinding,
        boxInstanceDescriptorBuffer.getDescriptorBufferBindingInfo()
    };

    vkCmdBindDescriptorBuffersEXT(cmd, 2, descriptorBufferBindingInfos.data());

    constexpr std::array<uint32_t, 2> indices{0, 1};
    const std::array offsets{
        drawInfo.sceneDataOffset,
        boxInstanceDescriptorBuffer.getDescriptorBufferSize() * drawInfo.currentFrameOverlap,
    };

    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 2, indices.data(), offsets.data());

    vkCmdBindVertexBuffers(cmd, 0, 1, &instancedVertexBuffer.buffer, &ZERO_DEVICE_SIZE);
    vkCmdBindIndexBuffer(cmd, instancedIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);


    vkCmdDrawIndexed(cmd, boxDrawIndexedData.indexCount, boxInstances.size(), boxDrawIndexedData.firstIndex
                     , boxDrawIndexedData.vertexOffset, boxDrawIndexedData.firstInstance);

    vkCmdEndRendering(cmd);

    clear();

}

void DebugRenderer::clear()
{
    boxInstances.clear();

    // remove anything past the defaults
    //vertices.clear();
    //indices.clear();
}

void DebugRenderer::createPipeline()
{
    resourceManager.destroyPipeline(pipeline);
    VkShaderModule vertShader = resourceManager.createShaderModule("shaders/debug/debug_renderer.vert");
    VkShaderModule fragShader = resourceManager.createShaderModule("shaders/debug/debug_renderer.frag");

    PipelineBuilder renderPipelineBuilder;
    VkVertexInputBindingDescription vertexBinding{};
    vertexBinding.binding = 0;
    vertexBinding.stride = sizeof(DebugRendererVertex);
    vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription vertexAttributes[4];
    vertexAttributes[0].binding = 0;
    vertexAttributes[0].location = 0;
    vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexAttributes[0].offset = offsetof(DebugRendererVertex, position);

    const VkVertexInputBindingDescription vertexBindings[1] = {vertexBinding};

    renderPipelineBuilder.setupVertexInput(vertexBindings, 1, vertexAttributes, 1);

    renderPipelineBuilder.setShaders(vertShader, fragShader);
    renderPipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
    renderPipelineBuilder.setupRasterization(VK_POLYGON_MODE_LINE, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    renderPipelineBuilder.disableMultisampling();
    renderPipelineBuilder.disableBlending();
    renderPipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    renderPipelineBuilder.setupRenderer({ALBEDO_FORMAT}, DEPTH_FORMAT);
    renderPipelineBuilder.setupPipelineLayout(pipelineLayout);
    const std::vector additionalDynamicStates{VK_DYNAMIC_STATE_LINE_WIDTH};
    pipeline = resourceManager.createRenderPipeline(renderPipelineBuilder, additionalDynamicStates);
    resourceManager.destroyShaderModule(vertShader);
    resourceManager.destroyShaderModule(fragShader);
}

void DebugRenderer::generateBuffers()
{
    // populate vertex and index buffers
}

void DebugRenderer::drawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color)
{}

void DebugRenderer::drawSphere(const glm::vec3& center, float radius, const glm::vec3& color, DebugRendererCategory category)
{
    if (!hasFlag(activeCategories, category)) { return; }


    // add to vertices and indices
}

void DebugRenderer::drawBox(const glm::vec3& center, const glm::vec3& dimensions, const glm::vec3& color, DebugRendererCategory category)
{
    if (!hasFlag(activeCategories, category)) { return; }

    const glm::mat4 transform = glm::translate(glm::mat4(1.0f), center) *
                                glm::scale(glm::mat4(1.0f), dimensions);

    BoxInstance instance{};
    instance.transform = transform;
    instance.color = color;

    boxInstances.push_back(instance);
}

void DebugRenderer::drawBoxMinmax(const glm::vec3& min, const glm::vec3& max, const glm::vec3& color, DebugRendererCategory category)
{
    if (!hasFlag(activeCategories, category)) { return; }

    // add box instance to
    std::vector<BoxInstance> boxInstances;
}

}
