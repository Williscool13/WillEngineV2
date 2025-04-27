//
// Created by William on 2025-04-27.
//

#include "debug_renderer.h"

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "src/renderer/resource_manager.h"


namespace will_engine::debug_renderer
{
DebugRenderer::DebugRenderer(ResourceManager& resourceManager) : resourceManager(resourceManager)
{
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

    boxDrawIndirect.indexCount = boxIndicesCount;
    boxDrawIndirect.instanceCount = 0;
    boxDrawIndirect.firstIndex = boxIndexOffset;
    boxDrawIndirect.vertexOffset = boxVertexOffset;
    boxDrawIndirect.firstInstance = 0;

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

    std::array<BufferCopyInfo, 5> bufferCopies = {
        BufferCopyInfo(instancedVertexStaging, 0, instancedVertexBuffer, 0, instancedVertexBufferSize),
        {instancedIndexStaging, 0, instancedIndexBuffer, 0, instancedIndexBufferSize},
    };

    resourceManager.copyBufferImmediate(bufferCopies);
    for (BufferCopyInfo bufferCopy : bufferCopies) {
        resourceManager.destroyBufferImmediate(bufferCopy.src);
    }

    // Box Instance Data Buffer
    boxInstanceBuffer = resourceManager.createHostSequentialBuffer(DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT * sizeof(BoxInstance));

    // Addresses Buffer - change number to be number of instance buffers to be in the addresses buffer
    size_t addressesBufferSize = sizeof(VkDeviceAddress) * 1;
    addressBuffer = resourceManager.createHostSequentialBuffer(addressesBufferSize);
    const std::array addresses = {resourceManager.getBufferAddress(boxInstanceBuffer)};
    memcpy(addressBuffer.info.pMappedData, addresses.data(), addressesBufferSize);

    // Draw indirect buffer
    drawIndirectBuffer = resourceManager.createHostSequentialBuffer(sizeof(VkDrawIndexedIndirectCommand) * 1);
    const std::array drawIndirectCommands = {boxDrawIndirect};
    memcpy(drawIndirectBuffer.info.pMappedData, drawIndirectCommands.data(), sizeof(VkDrawIndexedIndirectCommand) * 1);

    // todo: copy staging vertex and index into "instance real buffers"


    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    addressesLayout = resourceManager.createDescriptorSetLayout(layoutBuilder, VK_SHADER_STAGE_VERTEX_BIT,
                                                                VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    addressesDescriptorBuffer = resourceManager.createDescriptorBufferUniform(addressesLayout, 1);

    DescriptorUniformData addressesUniformData{
        .uniformBuffer = addressBuffer,
        .allocSize = addressesBufferSize,
    };
    resourceManager.setupDescriptorBufferUniform(addressesDescriptorBuffer, {addressesUniformData}, 0);
}

DebugRenderer::~DebugRenderer() {}

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

    // Create transformation matrix directly:
    // 1. Scale from unit cube to the desired dimensions
    // 2. Translate to the center position
    const glm::mat4 transform = glm::translate(glm::mat4(1.0f), center) *
                          glm::scale(glm::mat4(1.0f), dimensions);

    // Create and add the box instance
    BoxInstance instance;
    instance.transform = transform;
    instance.color = color;

    boxInstances.push_back(instance);


    // Update the indirect draw command

}

void DebugRenderer::drawBoxMinmax(const glm::vec3& min, const glm::vec3& max, const glm::vec3& color, DebugRendererCategory category)
{
    if (!hasFlag(activeCategories, category)) { return; }

    // add box instance to
    std::vector<BoxInstance> boxInstances;
}

void DebugRenderer::render(VkCommandBuffer cmd)
{
    if (boxInstances.size() > DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT) {
        boxInstances.resize(DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT);
    }

    boxDrawIndirect.instanceCount = boxInstances.size();

    const std::array drawIndirectCommands = {boxDrawIndirect};
    memcpy(drawIndirectBuffer.info.pMappedData, drawIndirectCommands.data(), sizeof(VkDrawIndexedIndirectCommand) * 1);

    memcpy(boxInstanceBuffer.info.pMappedData, boxInstances.data(), sizeof(BoxInstance) * boxInstances.size());

    // draw w/ pipeline
}

void DebugRenderer::clear()
{
    // remove anything past the defaults
    vertices.clear();
    indices.clear();
}

void DebugRenderer::createPipeline()
{}

void DebugRenderer::generateBuffers()
{
    // populate vertex and index buffers
}
}
