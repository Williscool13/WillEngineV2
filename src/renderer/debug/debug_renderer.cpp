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
    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    uniformLayout = resourceManager.createDescriptorSetLayout(layoutBuilder, VK_SHADER_STAGE_VERTEX_BIT,
                                                              VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    setupBoxRendering(BOX_INSTANCE_INDEX);
    setupSphereRendering(SPHERE_INSTANCE_INDEX);
    setupLineRendering();

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


    std::array<VkDescriptorSetLayout, 2> descriptorLayout;
    descriptorLayout[0] = resourceManager.getSceneDataLayout();
    descriptorLayout[1] = uniformLayout;

    VkPipelineLayoutCreateInfo layoutInfo = vk_helpers::pipelineLayoutCreateInfo();
    layoutInfo.pSetLayouts = descriptorLayout.data();
    layoutInfo.pNext = nullptr;
    layoutInfo.setLayoutCount = 2;
    layoutInfo.pPushConstantRanges = nullptr;
    layoutInfo.pushConstantRangeCount = 0;

    instancedPipelineLayout = resourceManager.createPipelineLayout(layoutInfo);

    layoutInfo.setLayoutCount = 1;
    normalPipelineLayout = resourceManager.createPipelineLayout(layoutInfo);

    createPipeline();
}

DebugRenderer::~DebugRenderer()
{
    resourceManager.destroyDescriptorSetLayout(uniformLayout);

    resourceManager.destroyBuffer(instancedVertexBuffer);
    resourceManager.destroyBuffer(instancedIndexBuffer);

    for (DebugRenderGroup& group : debugRenderInstanceGroups) {
        for (AllocatedBuffer& buffer : group.instanceBuffers) {
            resourceManager.destroyBuffer(buffer);
        }
        resourceManager.destroyDescriptorBuffer(group.instanceDescriptorBuffer);
    }

    for (AllocatedBuffer& buffer : lineVertexBuffers) {
        resourceManager.destroyBuffer(buffer);
    }

    resourceManager.destroyPipelineLayout(instancedPipelineLayout);
    resourceManager.destroyPipelineLayout(normalPipelineLayout);
    resourceManager.destroyPipeline(instancedLinePipeline);
    resourceManager.destroyPipeline(linePipeline);
    resourceManager.destroyPipeline(trianglePipeline);
}

void DebugRenderer::drawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color)
{
    DebugRenderer* inst = get();
    if (!inst) { return; }
    inst->drawLineImpl(start, end, color);
}

void DebugRenderer::drawSphere(const glm::vec3& center, float radius, const glm::vec3& color, DebugRendererCategory category)
{
    DebugRenderer* inst = get();
    if (!inst) { return; }
    inst->drawSphereImpl(center, radius, color, category);
}

void DebugRenderer::drawBox(const glm::vec3& center, const glm::vec3& dimensions, const glm::vec3& color, DebugRendererCategory category)
{
    DebugRenderer* inst = get();
    if (!inst) { return; }
    inst->drawBoxImpl(center, dimensions, color, category);
}

void DebugRenderer::drawBoxMinMax(const glm::vec3& min, const glm::vec3& max, const glm::vec3& color, DebugRendererCategory category)
{
    DebugRenderer* inst = get();
    if (!inst) { return; }
    inst->drawBoxMinMaxImpl(min, max, color, category);
}

void DebugRenderer::draw(VkCommandBuffer cmd, const DebugRendererDrawInfo& drawInfo)
{
    if (drawInfo.currentFrameOverlap < 0 || drawInfo.currentFrameOverlap >= FRAME_OVERLAP) { return; }

    // Upload
    for (DebugRenderGroup& group : debugRenderInstanceGroups) {
        if (group.instances.empty()) { continue; }

        AllocatedBuffer& instanceBuffer = group.instanceBuffers[drawInfo.currentFrameOverlap];

        if (group.instances.size() > group.instanceBufferSizes[drawInfo.currentFrameOverlap]) {
            uint64_t newSize = group.instanceBufferSizes[drawInfo.currentFrameOverlap];
            // Can potentially overflow resulting in infinite loop, but it would need to be so insanely large, cant even create a buffer that big
            while (group.instances.size() > newSize) {
                newSize += DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT;
            }

            resourceManager.destroyBuffer(instanceBuffer);

            // Don't need to copy, writing to it in the next section anyway
            const uint64_t newBufferSize = newSize * sizeof(DebugRendererInstance);
            instanceBuffer = resourceManager.createHostSequentialBuffer(newBufferSize);

            DescriptorUniformData addressesUniformData{
                .uniformBuffer = instanceBuffer,
                .allocSize = newBufferSize,
            };

            resourceManager.setupDescriptorBufferUniform(group.instanceDescriptorBuffer, {addressesUniformData}, drawInfo.currentFrameOverlap);

            group.instanceBufferSizes[drawInfo.currentFrameOverlap] = newSize;
        }

        memcpy(instanceBuffer.info.pMappedData, group.instances.data(), sizeof(DebugRendererInstance) * group.instances.size());
    }

    // Upload Vertex Line Data
    {
        AllocatedBuffer& lineVertexBuffer = lineVertexBuffers[drawInfo.currentFrameOverlap];

        if (lineVertices.size() > lineVertexBuffersSizes[drawInfo.currentFrameOverlap]) {
            int64_t newSize = lineVertexBuffersSizes[drawInfo.currentFrameOverlap];
            while (lineVertices.size() > newSize) {
                newSize += DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT;
            }

            resourceManager.destroyBuffer(lineVertexBuffer);

            const uint64_t newBufferSize = newSize * sizeof(DebugRendererVertexFull);
            lineVertexBuffer = resourceManager.createHostSequentialBuffer(newBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
            lineVertexBuffersSizes[drawInfo.currentFrameOverlap] = newSize;
        }

        memcpy(lineVertexBuffer.info.pMappedData, lineVertices.data(), sizeof(DebugRendererVertexFull) * lineVertices.size());
    }


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
    // Instanced rendering
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, instancedLinePipeline);

        vkCmdSetLineWidth(cmd, 2.0f);

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

        for (DebugRenderGroup& group : debugRenderInstanceGroups) {
            if (group.instances.empty()) { continue;; }


            const std::array descriptorBufferBindingInfos{
                drawInfo.sceneDataBinding,
                group.instanceDescriptorBuffer.getDescriptorBufferBindingInfo()
            };

            vkCmdBindDescriptorBuffersEXT(cmd, 2, descriptorBufferBindingInfos.data());

            constexpr std::array<uint32_t, 2> indices{0, 1};
            const std::array offsets{
                drawInfo.sceneDataOffset,
                group.instanceDescriptorBuffer.getDescriptorBufferSize() * drawInfo.currentFrameOverlap,
            };

            vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, instancedPipelineLayout, 0, 2, indices.data(), offsets.data());

            vkCmdBindVertexBuffers(cmd, 0, 1, &instancedVertexBuffer.buffer, &ZERO_DEVICE_SIZE);
            vkCmdBindIndexBuffer(cmd, instancedIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);


            vkCmdDrawIndexed(cmd, group.drawIndexedData.indexCount, group.instances.size(), group.drawIndexedData.firstIndex
                             , group.drawIndexedData.vertexOffset, group.drawIndexedData.firstInstance);
        }
    }

    // Line Rendering
    if (lineVertices.size() > 0) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline);

        vkCmdSetLineWidth(cmd, 2.0f);

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

        AllocatedBuffer& currentLineVertexBuffer = lineVertexBuffers[drawInfo.currentFrameOverlap];
        vkCmdBindVertexBuffers(cmd, 0, 1, &currentLineVertexBuffer.buffer, &ZERO_DEVICE_SIZE);
        vkCmdDraw(cmd, lineVertices.size(), 1, 0, 0);
    }

    vkCmdEndRendering(cmd);

    clear();
}

void DebugRenderer::clear()
{
    for (DebugRenderGroup& group : debugRenderInstanceGroups) {
        group.instances.clear();
    }
    lineVertices.clear();
}

void DebugRenderer::createPipeline()
{
    resourceManager.destroyPipeline(instancedLinePipeline);
    resourceManager.destroyPipeline(trianglePipeline);
    resourceManager.destroyPipeline(linePipeline);

    // Instanced Pipeline
    {
        VkShaderModule vertShader = resourceManager.createShaderModule("shaders/debug/debug_renderer_instanced.vert");
        VkShaderModule fragShader = resourceManager.createShaderModule("shaders/debug/debug_renderer_instanced.frag");

        PipelineBuilder renderPipelineBuilder;
        VkVertexInputBindingDescription vertexBinding{};
        vertexBinding.binding = 0;
        vertexBinding.stride = sizeof(DebugRendererVertex);
        vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::array<VkVertexInputAttributeDescription, 1> vertexAttributes;
        vertexAttributes[0].binding = 0;
        vertexAttributes[0].location = 0;
        vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        vertexAttributes[0].offset = offsetof(DebugRendererVertex, position);

        renderPipelineBuilder.setupVertexInput(&vertexBinding, 1, vertexAttributes.data(), 1);

        renderPipelineBuilder.setShaders(vertShader, fragShader);
        renderPipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_LINE_LIST); // line so that we don't see diagonals on quads
        renderPipelineBuilder.setupRasterization(VK_POLYGON_MODE_LINE, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
        renderPipelineBuilder.disableMultisampling();
        renderPipelineBuilder.disableBlending();
        renderPipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
        renderPipelineBuilder.setupRenderer({ALBEDO_FORMAT}, DEPTH_FORMAT);
        renderPipelineBuilder.setupPipelineLayout(instancedPipelineLayout);
        const std::vector additionalDynamicStates{VK_DYNAMIC_STATE_LINE_WIDTH};
        instancedLinePipeline = resourceManager.createRenderPipeline(renderPipelineBuilder, additionalDynamicStates);

        resourceManager.destroyShaderModule(vertShader);
        resourceManager.destroyShaderModule(fragShader);
    }

    // Line Vertex Pipeline
    {
        VkShaderModule vertShader = resourceManager.createShaderModule("shaders/debug/debug_renderer.vert");
        VkShaderModule fragShader = resourceManager.createShaderModule("shaders/debug/debug_renderer.frag");

        PipelineBuilder renderPipelineBuilder;
        VkVertexInputBindingDescription vertexBinding{};
        vertexBinding.binding = 0;
        vertexBinding.stride = sizeof(DebugRendererVertexFull);
        vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::array<VkVertexInputAttributeDescription, 2> vertexAttributes;
        vertexAttributes[0].binding = 0;
        vertexAttributes[0].location = 0;
        vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        vertexAttributes[0].offset = offsetof(DebugRendererVertexFull, position);

        vertexAttributes[1].binding = 0;
        vertexAttributes[1].location = 1;
        vertexAttributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        vertexAttributes[1].offset = offsetof(DebugRendererVertexFull, color);

        renderPipelineBuilder.setupVertexInput(&vertexBinding, 1, vertexAttributes.data(), 2);

        renderPipelineBuilder.setShaders(vertShader, fragShader);
        renderPipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
        renderPipelineBuilder.setupRasterization(VK_POLYGON_MODE_LINE, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
        renderPipelineBuilder.disableMultisampling();
        renderPipelineBuilder.disableBlending();
        renderPipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
        renderPipelineBuilder.setupRenderer({ALBEDO_FORMAT}, DEPTH_FORMAT);
        renderPipelineBuilder.setupPipelineLayout(normalPipelineLayout);
        const std::vector additionalDynamicStates{VK_DYNAMIC_STATE_LINE_WIDTH};
        linePipeline = resourceManager.createRenderPipeline(renderPipelineBuilder, additionalDynamicStates);

        resourceManager.destroyShaderModule(vertShader);
        resourceManager.destroyShaderModule(fragShader);
    }

    //renderPipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    //trianglePipeline = resourceManager.createRenderPipeline(renderPipelineBuilder, additionalDynamicStates);
}

void DebugRenderer::drawLineImpl(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color)
{
    lineVertices.push_back({start, color});
    lineVertices.push_back({end, color});
}

void DebugRenderer::drawSphereImpl(const glm::vec3& center, const float radius, const glm::vec3& color, DebugRendererCategory category)
{
    if (!hasFlag(activeCategories, category)) { return; }

    const glm::mat4 transform = glm::translate(glm::mat4(1.0f), center) * glm::scale(glm::mat4(1.0f), glm::vec3(radius));

    DebugRendererInstance instance{};
    instance.transform = transform;
    instance.color = color;

    debugRenderInstanceGroups[SPHERE_INSTANCE_INDEX].instances.push_back(instance);
}

void DebugRenderer::drawBoxImpl(const glm::vec3& center, const glm::vec3& dimensions, const glm::vec3& color, const DebugRendererCategory category)
{
    if (!hasFlag(activeCategories, category)) { return; }

    const glm::mat4 transform = glm::translate(glm::mat4(1.0f), center) * glm::scale(glm::mat4(1.0f), dimensions);

    DebugRendererInstance instance{};
    instance.transform = transform;
    instance.color = color;

    debugRenderInstanceGroups[BOX_INSTANCE_INDEX].instances.push_back(instance);
}

void DebugRenderer::drawBoxMinMaxImpl(const glm::vec3& min, const glm::vec3& max, const glm::vec3& color, const DebugRendererCategory category)
{
    if (!hasFlag(activeCategories, category)) { return; }

    const glm::vec3 size = max - min;
    const glm::vec3 center = min + size * 0.5f;

    drawBoxImpl(center, size, color, category);
}

void DebugRenderer::setupBoxRendering(const int32_t index)
{
    // Box Instance Data Buffer
    constexpr uint64_t boxInstanceBufferSize = DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT * sizeof(DebugRendererInstance);

    debugRenderInstanceGroups[index].instanceDescriptorBuffer = resourceManager.createDescriptorBufferUniform(uniformLayout, FRAME_OVERLAP);
    for (int32_t i = 0; i < FRAME_OVERLAP; ++i) {
        debugRenderInstanceGroups[index].instanceBufferSizes[i] = DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT;
        debugRenderInstanceGroups[index].instanceBuffers[i] = resourceManager.createHostSequentialBuffer(boxInstanceBufferSize);
        DescriptorUniformData addressesUniformData{
            .uniformBuffer = debugRenderInstanceGroups[index].instanceBuffers[i],
            .allocSize = boxInstanceBufferSize,
        };

        resourceManager.setupDescriptorBufferUniform(debugRenderInstanceGroups[index].instanceDescriptorBuffer, {addressesUniformData}, i);
    }


    debugRenderInstanceGroups[index].instances.reserve(DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT);
    // 8 vertices, 12 edges (24 points)
    constexpr int32_t boxVertexCount = 8;
    constexpr int32_t boxIndicesCount = 24;
    instancedVertices.reserve(instancedVertices.size() + boxVertexCount);
    instancedIndices.reserve(instancedIndices.size() + boxIndicesCount);

    std::vector<DebugRendererVertex> boxVertices = {
        {{-0.5, -0.5, -0.5}}, // 0: near bottom left
        {{ 0.5, -0.5, -0.5}}, // 1: near bottom right
        {{ 0.5,  0.5, -0.5}}, // 2: near top right
        {{-0.5,  0.5, -0.5}}, // 3: near top left
        {{-0.5, -0.5,  0.5}}, // 4: far bottom left
        {{ 0.5, -0.5,  0.5}}, // 5: far bottom right
        {{ 0.5,  0.5,  0.5}}, // 6: far top right
        {{-0.5,  0.5,  0.5}}  // 7: far top left
    };

    std::vector<uint32_t> boxIndices = {
        // Near face
        0, 1, 1, 2, 2, 3, 3, 0,
        // Far face
        4, 5, 5, 6, 6, 7, 7, 4,
        // Connecting edges
        0, 4, 1, 5, 2, 6, 3, 7
    };

    const size_t boxVertexOffset = instancedVertices.size();
    const size_t boxIndexOffset = instancedIndices.size();
    instancedVertices.insert(instancedVertices.end(), boxVertices.begin(), boxVertices.end());
    instancedIndices.insert(instancedIndices.end(), boxIndices.begin(), boxIndices.end());

    debugRenderInstanceGroups[index].drawIndexedData.indexCount = boxIndicesCount;
    debugRenderInstanceGroups[index].drawIndexedData.firstIndex = boxIndexOffset;
    debugRenderInstanceGroups[index].drawIndexedData.vertexOffset = static_cast<int32_t>(boxVertexOffset);
    debugRenderInstanceGroups[index].drawIndexedData.firstInstance = 0;
}

void DebugRenderer::setupSphereRendering(const int32_t index)
{
    // Sphere Instance Data Buffer
    constexpr uint64_t sphereInstanceBufferSize = DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT * sizeof(DebugRendererInstance);

    debugRenderInstanceGroups[index].instanceDescriptorBuffer = resourceManager.createDescriptorBufferUniform(uniformLayout, FRAME_OVERLAP);
    for (int32_t i = 0; i < FRAME_OVERLAP; ++i) {
        debugRenderInstanceGroups[index].instanceBuffers[i] = resourceManager.createHostSequentialBuffer(sphereInstanceBufferSize);
        debugRenderInstanceGroups[index].instanceBufferSizes[i] = DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT;
        DescriptorUniformData addressesUniformData{
            .uniformBuffer = debugRenderInstanceGroups[index].instanceBuffers[i],
            .allocSize = sphereInstanceBufferSize,
        };

        resourceManager.setupDescriptorBufferUniform(debugRenderInstanceGroups[index].instanceDescriptorBuffer, {addressesUniformData}, i);
    }

    debugRenderInstanceGroups[index].instances.reserve(DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT);

    constexpr int32_t rings = SPHERE_DETAIL_MEDIUM.rings;
    constexpr int32_t segments = SPHERE_DETAIL_MEDIUM.segments;
    constexpr int32_t sphereVertexCount = (rings + 1) * segments;
    constexpr int32_t sphereLineCount = rings * segments * 2; // Longitude + latitude lines
    constexpr int32_t sphereIndicesCount = sphereLineCount * 2; // 2 indices per line

    instancedVertices.reserve(instancedVertices.size() + sphereVertexCount);
    instancedIndices.reserve(instancedIndices.size() + sphereIndicesCount);

    std::vector<DebugRendererVertex> sphereVertices;
    sphereVertices.reserve(sphereVertexCount);

    for (int32_t ring = 0; ring <= rings; ++ring) {
        float phi = glm::pi<float>() * static_cast<float>(ring) / static_cast<float>(rings);
        for (int32_t segment = 0; segment < segments; ++segment) {
            float theta = 2.0f * glm::pi<float>() * static_cast<float>(segment) / static_cast<float>(segments);

            float x = std::sin(phi) * std::cos(theta);
            float y = std::cos(phi);
            float z = std::sin(phi) * std::sin(theta);

            sphereVertices.push_back({{x, y, z}});
        }
    }

    std::vector<uint32_t> sphereIndices;
    sphereIndices.reserve(sphereIndicesCount);

    for (int32_t ring = 0; ring < rings; ++ring) {
        const int32_t ringStart = ring * segments;
        const int32_t nextRingStart = (ring + 1) * segments;
        for (int32_t segment = 0; segment < segments; ++segment) {
            sphereIndices.push_back(ringStart + segment);
            sphereIndices.push_back(nextRingStart + segment);
        }
    }

    for (int32_t ring = 0; ring <= rings; ++ring) {
        const int32_t ringStart = ring * segments;
        for (int32_t segment = 0; segment < segments; ++segment) {
            sphereIndices.push_back(ringStart + segment);
            sphereIndices.push_back(ringStart + ((segment + 1) % segments));
        }
    }

    const size_t sphereVertexOffset = instancedVertices.size();
    const size_t sphereIndexOffset = instancedIndices.size();
    instancedVertices.insert(instancedVertices.end(), sphereVertices.begin(), sphereVertices.end());
    instancedIndices.insert(instancedIndices.end(), sphereIndices.begin(), sphereIndices.end());

    debugRenderInstanceGroups[index].drawIndexedData.indexCount = sphereIndicesCount;
    debugRenderInstanceGroups[index].drawIndexedData.firstIndex = sphereIndexOffset;
    debugRenderInstanceGroups[index].drawIndexedData.vertexOffset = static_cast<int32_t>(sphereVertexOffset);
    debugRenderInstanceGroups[index].drawIndexedData.firstInstance = 0;
}

void DebugRenderer::setupLineRendering()
{
    constexpr uint64_t vertexBufferSize = DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT * sizeof(DebugRendererVertexFull);
    for (int32_t i{0}; i < FRAME_OVERLAP; i++) {
        lineVertexBuffersSizes[i] = DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT;
        lineVertexBuffers[i] = resourceManager.createHostSequentialBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    }
}
}
