//
// Created by William on 2025-04-27.
//


#include "debug_renderer.h"

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <volk/volk.h>

#include "engine/renderer/resource_manager.h"

#if WILL_ENGINE_DEBUG_DRAW

namespace will_engine::renderer
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
    Buffer instancedVertexStaging = resourceManager.createStagingBuffer(instancedVertexBufferSize);
    memcpy(instancedVertexStaging.info.pMappedData, instancedVertices.data(), instancedVertexBufferSize);
    instancedVertexBuffer = resourceManager.createDeviceBuffer(instancedVertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    // Index Buffer
    const uint64_t instancedIndexBufferSize = instancedIndices.size() * sizeof(uint32_t);
    Buffer instancedIndexStaging = resourceManager.createStagingBuffer(instancedIndexBufferSize);
    memcpy(instancedIndexStaging.info.pMappedData, instancedIndices.data(), instancedIndexBufferSize);
    instancedIndexBuffer = resourceManager.createDeviceBuffer(instancedIndexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    std::array<BufferCopyInfo, 2> bufferCopies = {
        BufferCopyInfo(instancedVertexStaging.buffer, 0, instancedVertexBuffer.buffer, 0, instancedVertexBufferSize),
        {instancedIndexStaging.buffer, 0, instancedIndexBuffer.buffer, 0, instancedIndexBufferSize},
    };

    resourceManager.copyBufferImmediate(bufferCopies);
    resourceManager.destroyResourceImmediate(std::move(instancedIndexStaging));
    resourceManager.destroyResourceImmediate(std::move(instancedVertexStaging));


    std::array<VkDescriptorSetLayout, 2> descriptorLayout;
    descriptorLayout[0] = resourceManager.getSceneDataLayout();
    descriptorLayout[1] = uniformLayout.layout;

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
    resourceManager.destroyResource(std::move(uniformLayout));

    resourceManager.destroyResource(std::move(instancedVertexBuffer));
    resourceManager.destroyResource(std::move(instancedIndexBuffer));

    for (DebugRenderGroup& group : debugRenderInstanceGroups) {
        for (Buffer& buffer : group.instanceBuffers) {
            resourceManager.destroyResource(std::move(buffer));
        }
        resourceManager.destroyResource(std::move(group.instanceDescriptorBuffer));
    }

    for (Buffer& buffer : lineVertexBuffers) {
        resourceManager.destroyResource(std::move(buffer));
    }
    for (Buffer& buffer : triangleVertexBuffers) {
        resourceManager.destroyResource(std::move(buffer));
    }

    resourceManager.destroyResource(std::move(instancedPipelineLayout));
    resourceManager.destroyResource(std::move(normalPipelineLayout));
    resourceManager.destroyResource(std::move(instancedLinePipeline));
    resourceManager.destroyResource(std::move(linePipeline));
    resourceManager.destroyResource(std::move(trianglePipeline));
}

void DebugRenderer::drawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color)
{
    DebugRenderer* inst = get();
    if (!inst) { return; }
    inst->drawLineImpl(start, end, color);
}

void DebugRenderer::drawTriangle(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec3& color)
{
    DebugRenderer* inst = get();
    if (!inst) { return; }
    inst->drawTriangleImpl(v1, v2, v3, color);
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

        Buffer& instanceBuffer = group.instanceBuffers[drawInfo.currentFrameOverlap];

        if (group.instances.size() > group.instanceBufferSizes[drawInfo.currentFrameOverlap]) {
            uint64_t newSize = group.instanceBufferSizes[drawInfo.currentFrameOverlap];
            // Can potentially overflow resulting in infinite loop, but it would need to be so insanely large, cant even create a buffer that big
            while (group.instances.size() > newSize) {
                newSize += DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT;
            }

            resourceManager.destroyResource(std::move(instanceBuffer));

            // Don't need to copy, writing to it in the next section anyway
            const uint64_t newBufferSize = newSize * sizeof(DebugRendererInstance);
            instanceBuffer = resourceManager.createHostSequentialBuffer(newBufferSize);

            DescriptorUniformData addressesUniformData{
                .buffer = instanceBuffer.buffer,
                .allocSize = newBufferSize,
            };

            resourceManager.setupDescriptorBufferUniform(group.instanceDescriptorBuffer, {addressesUniformData}, drawInfo.currentFrameOverlap);

            group.instanceBufferSizes[drawInfo.currentFrameOverlap] = newSize;
        }

        memcpy(instanceBuffer.info.pMappedData, group.instances.data(), sizeof(DebugRendererInstance) * group.instances.size());
    }

    // Upload Vertex Data
    {
        Buffer& lineVertexBuffer = lineVertexBuffers[drawInfo.currentFrameOverlap];

        if (lineVertices.size() > lineVertexBufferSizes[drawInfo.currentFrameOverlap]) {
            int64_t newSize = lineVertexBufferSizes[drawInfo.currentFrameOverlap];
            while (lineVertices.size() > newSize) {
                newSize += DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT;
            }

            resourceManager.destroyResource(std::move(lineVertexBuffer));

            const uint64_t newBufferSize = newSize * sizeof(DebugRendererVertexFull);
            lineVertexBuffer = resourceManager.createHostSequentialBuffer(newBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
            lineVertexBufferSizes[drawInfo.currentFrameOverlap] = newSize;
        }

        memcpy(lineVertexBuffer.info.pMappedData, lineVertices.data(), sizeof(DebugRendererVertexFull) * lineVertices.size());

        Buffer& triangleVertexBuffer = triangleVertexBuffers[drawInfo.currentFrameOverlap];

        if (triangleVertices.size() > triangleVertexBufferSizes[drawInfo.currentFrameOverlap]) {
            int64_t newSize = triangleVertexBufferSizes[drawInfo.currentFrameOverlap];
            while (triangleVertices.size() > newSize) {
                newSize += DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT;
            }

            resourceManager.destroyResource(std::move(triangleVertexBuffer));

            const uint64_t newBufferSize = newSize * sizeof(DebugRendererVertexFull);
            triangleVertexBuffer = resourceManager.createHostSequentialBuffer(newBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
            triangleVertexBufferSizes[drawInfo.currentFrameOverlap] = newSize;
        }

        memcpy(triangleVertexBuffer.info.pMappedData, triangleVertices.data(), sizeof(DebugRendererVertexFull) * triangleVertices.size());
    }


    const VkRenderingAttachmentInfo imageAttachment = vk_helpers::attachmentInfo(drawInfo.debugTarget, nullptr,
                                                                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const VkRenderingAttachmentInfo depthAttachment = vk_helpers::attachmentInfo(drawInfo.depthTarget, nullptr,
                                                                                 VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.pNext = nullptr;

    VkRenderingAttachmentInfo renderAttachments[1];
    renderAttachments[0] = imageAttachment;

    renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, RENDER_EXTENTS};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = renderAttachments;
    renderInfo.pDepthAttachment = &depthAttachment;
    renderInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(cmd, &renderInfo);

    vkCmdSetLineWidth(cmd, 1.0f);

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

    // Instanced rendering
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, instancedLinePipeline.pipeline);

        for (DebugRenderGroup& group : debugRenderInstanceGroups) {
            if (group.instances.empty()) { continue;; }


            const std::array<VkDescriptorBufferBindingInfoEXT, 2> descriptorBufferBindingInfos{
                drawInfo.sceneDataBinding,
                group.instanceDescriptorBuffer.getBindingInfo()
            };

            vkCmdBindDescriptorBuffersEXT(cmd, 2, descriptorBufferBindingInfos.data());

            constexpr std::array<uint32_t, 2> indices{0, 1};
            const std::array<VkDeviceSize, 2> offsets{
                drawInfo.sceneDataOffset,
                group.instanceDescriptorBuffer.getDescriptorBufferSize() * drawInfo.currentFrameOverlap,
            };

            vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, instancedPipelineLayout.layout, 0, 2, indices.data(),
                                               offsets.data());

            vkCmdBindVertexBuffers(cmd, 0, 1, &instancedVertexBuffer.buffer, &ZERO_DEVICE_SIZE);
            vkCmdBindIndexBuffer(cmd, instancedIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);


            vkCmdDrawIndexed(cmd, group.drawIndexedData.indexCount, group.instances.size(), group.drawIndexedData.firstIndex
                             , group.drawIndexedData.vertexOffset, group.drawIndexedData.firstInstance);
        }
    }

    // Line Rendering
    if (!lineVertices.empty()) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline.pipeline);

        const std::array descriptorBufferBindingInfos{drawInfo.sceneDataBinding};
        constexpr std::array<uint32_t, 1> indices{};
        const std::array offsets{drawInfo.sceneDataOffset};
        vkCmdBindDescriptorBuffersEXT(cmd, 1, descriptorBufferBindingInfos.data());
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, normalPipelineLayout.layout, 0, 1, indices.data(), offsets.data());

        Buffer& currentLineVertexBuffer = lineVertexBuffers[drawInfo.currentFrameOverlap];
        vkCmdBindVertexBuffers(cmd, 0, 1, &currentLineVertexBuffer.buffer, &ZERO_DEVICE_SIZE);
        vkCmdDraw(cmd, lineVertices.size(), 1, 0, 0);
    }

    if (!triangleVertices.empty()) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline.pipeline);

        const std::array descriptorBufferBindingInfos{drawInfo.sceneDataBinding};
        constexpr std::array<uint32_t, 1> indices{};
        const std::array offsets{drawInfo.sceneDataOffset};
        vkCmdBindDescriptorBuffersEXT(cmd, 1, descriptorBufferBindingInfos.data());
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, normalPipelineLayout.layout, 0, 1, indices.data(), offsets.data());

        Buffer& currentTriangleVertexBuffer = triangleVertexBuffers[drawInfo.currentFrameOverlap];
        vkCmdBindVertexBuffers(cmd, 0, 1, &currentTriangleVertexBuffer.buffer, &ZERO_DEVICE_SIZE);
        vkCmdDraw(cmd, triangleVertices.size(), 1, 0, 0);
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
    triangleVertices.clear();
}

void DebugRenderer::createPipeline()
{
    resourceManager.destroyResource(std::move(instancedLinePipeline));
    resourceManager.destroyResource(std::move(trianglePipeline));
    resourceManager.destroyResource(std::move(linePipeline));

    // Instanced Pipeline
    {
        VkShaderModule vertShader = resourceManager.createShaderModule("shaders/debug/debug_renderer_instanced.vert");
        VkShaderModule fragShader = resourceManager.createShaderModule("shaders/debug/debug_renderer_instanced.frag");

        RenderPipelineBuilder renderPipelineBuilder;

        const std::vector<VkVertexInputBindingDescription> vertexBindings{
            {
                .binding = 0,
                .stride = sizeof(DebugRendererVertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            }
        };

        const std::vector<VkVertexInputAttributeDescription> vertexAttributes{
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(DebugRendererVertex, position),
            }
        };

        renderPipelineBuilder.setupVertexInput(vertexBindings, vertexAttributes);

        renderPipelineBuilder.setShaders(vertShader, fragShader);
        renderPipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_LINE_LIST); // line so that we don't see diagonals on quads
        renderPipelineBuilder.setupRasterization(VK_POLYGON_MODE_LINE, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
        renderPipelineBuilder.disableMultisampling();
        renderPipelineBuilder.disableBlending();
        renderPipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
        renderPipelineBuilder.setupRenderer({DEBUG_FORMAT}, DEPTH_STENCIL_FORMAT);
        renderPipelineBuilder.setupPipelineLayout(instancedPipelineLayout.layout);
        renderPipelineBuilder.addDynamicState(VK_DYNAMIC_STATE_LINE_WIDTH);
        instancedLinePipeline = resourceManager.createRenderPipeline(renderPipelineBuilder);

        resourceManager.destroyShaderModule(vertShader);
        resourceManager.destroyShaderModule(fragShader);
    }

    // Line Vertex Pipeline
    {
        VkShaderModule vertShader = resourceManager.createShaderModule("shaders/debug/debug_renderer.vert");
        VkShaderModule fragShader = resourceManager.createShaderModule("shaders/debug/debug_renderer.frag");

        RenderPipelineBuilder renderPipelineBuilder;

        const std::vector<VkVertexInputBindingDescription> vertexBindings{
            {
                .binding = 0,
                .stride = sizeof(DebugRendererVertexFull),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            }
        };

        const std::vector<VkVertexInputAttributeDescription> vertexAttributes{
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(DebugRendererVertexFull, position),
            },
            {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(DebugRendererVertexFull, color),
            }
        };

        renderPipelineBuilder.setupVertexInput(vertexBindings, vertexAttributes);

        renderPipelineBuilder.setShaders(vertShader, fragShader);
        renderPipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
        renderPipelineBuilder.setupRasterization(VK_POLYGON_MODE_LINE, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
        renderPipelineBuilder.disableMultisampling();
        renderPipelineBuilder.disableBlending();
        renderPipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
        renderPipelineBuilder.setupRenderer({DEBUG_FORMAT}, DEPTH_STENCIL_FORMAT);
        renderPipelineBuilder.setupPipelineLayout(normalPipelineLayout.layout);
        renderPipelineBuilder.addDynamicState(VK_DYNAMIC_STATE_LINE_WIDTH);
        linePipeline = resourceManager.createRenderPipeline(renderPipelineBuilder);

        //renderPipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
        renderPipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        trianglePipeline = resourceManager.createRenderPipeline(renderPipelineBuilder);

        resourceManager.destroyShaderModule(vertShader);
        resourceManager.destroyShaderModule(fragShader);
    }
}

void DebugRenderer::drawLineImpl(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color)
{
    lineVertices.push_back({start, color});
    lineVertices.push_back({end, color});
}

void DebugRenderer::drawTriangleImpl(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec3& color)
{
    triangleVertices.push_back({v1, color});
    triangleVertices.push_back({v2, color});
    triangleVertices.push_back({v3, color});
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

    debugRenderInstanceGroups[index].instanceDescriptorBuffer = resourceManager.createDescriptorBufferUniform(uniformLayout.layout, FRAME_OVERLAP);
    for (int32_t i = 0; i < FRAME_OVERLAP; ++i) {
        debugRenderInstanceGroups[index].instanceBufferSizes[i] = DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT;
        debugRenderInstanceGroups[index].instanceBuffers[i] = resourceManager.createHostSequentialBuffer(boxInstanceBufferSize);
        DescriptorUniformData addressesUniformData{
            .buffer = debugRenderInstanceGroups[index].instanceBuffers[i].buffer,
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
        {{0.5, -0.5, -0.5}}, // 1: near bottom right
        {{0.5, 0.5, -0.5}}, // 2: near top right
        {{-0.5, 0.5, -0.5}}, // 3: near top left
        {{-0.5, -0.5, 0.5}}, // 4: far bottom left
        {{0.5, -0.5, 0.5}}, // 5: far bottom right
        {{0.5, 0.5, 0.5}}, // 6: far top right
        {{-0.5, 0.5, 0.5}} // 7: far top left
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

    debugRenderInstanceGroups[index].instanceDescriptorBuffer = resourceManager.createDescriptorBufferUniform(uniformLayout.layout, FRAME_OVERLAP);
    for (int32_t i = 0; i < FRAME_OVERLAP; ++i) {
        debugRenderInstanceGroups[index].instanceBuffers[i] = resourceManager.createHostSequentialBuffer(sphereInstanceBufferSize);
        debugRenderInstanceGroups[index].instanceBufferSizes[i] = DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT;
        DescriptorUniformData addressesUniformData{
            .buffer = debugRenderInstanceGroups[index].instanceBuffers[i].buffer,
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
        lineVertexBufferSizes[i] = DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT;
        lineVertexBuffers[i] = resourceManager.createHostSequentialBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    }
}

void DebugRenderer::setupTriangleRendering()
{
    constexpr uint64_t vertexBufferSize = DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT * sizeof(DebugRendererVertexFull);
    for (int32_t i{0}; i < FRAME_OVERLAP; i++) {
        triangleVertexBufferSizes[i] = DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT;
        triangleVertexBuffers[i] = resourceManager.createHostSequentialBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    }
}
}
#endif
