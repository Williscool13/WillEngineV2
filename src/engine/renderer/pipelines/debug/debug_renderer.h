//
// Created by William on 2025-04-27.
//

#ifndef DEBUG_RENDERER_H
#define DEBUG_RENDERER_H

#include <array>

#include "debug_renderer_types.h"
#include "engine/renderer/resources/resources_fwd.h"

#ifndef WILL_ENGINE_DEBUG_DRAW
    #error This file should only be included when WILL_ENGINE_DEBUG is defined
#endif // !WILL_ENGINE_DEBUG

namespace will_engine::renderer
{
class ResourceManager;

struct DebugRendererPushConstant
{
    int32_t bInstanced{false};
};

/**
 * Note: Debug rendering doesn't write to the velocity buffer, so do not contribute to the velocity buffer. As a result, no TAA is expected.
 */
class DebugRenderer
{
public:
    static DebugRenderer* debugRenderer;
    static DebugRenderer* get() { return debugRenderer; }
    static void set(DebugRenderer* _debugRenderer) { debugRenderer = _debugRenderer; }


    void setupBoxRendering(int32_t index);

    void setupSphereRendering(int32_t index);

    void setupLineRendering();

    void setupTriangleRendering();

    explicit DebugRenderer(ResourceManager& resourceManager);

    ~DebugRenderer();

public:
    static void drawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color);

    static void drawTriangle(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec3& color);

    static void drawSphere(const glm::vec3& center, float radius, const glm::vec3& color, DebugRendererCategory category = DebugRendererCategory::General);

    static void drawBox(const glm::vec3& center, const glm::vec3& dimensions, const glm::vec3& color, DebugRendererCategory category = DebugRendererCategory::General);

    static void drawBoxMinMax(const glm::vec3& min, const glm::vec3& max, const glm::vec3& color, DebugRendererCategory category = DebugRendererCategory::General);

private:
    void drawLineImpl(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color);

    void drawTriangleImpl(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec3& color);

    void drawSphereImpl(const glm::vec3& center, float radius, const glm::vec3& color, DebugRendererCategory category = DebugRendererCategory::General);

    void drawBoxImpl(const glm::vec3& center, const glm::vec3& dimensions, const glm::vec3& color, DebugRendererCategory category = DebugRendererCategory::General);

    void drawBoxMinMaxImpl(const glm::vec3& min, const glm::vec3& max, const glm::vec3& color, DebugRendererCategory category = DebugRendererCategory::General);

public:
    void draw(VkCommandBuffer cmd, const DebugRendererDrawInfo& drawInfo);

    void reloadShaders()
    {
        createPipeline();
    }

private:
    void clear();

    void createPipeline();

    PipelineLayoutPtr instancedPipelineLayout{};
    PipelineLayoutPtr normalPipelineLayout{};
    PipelinePtr instancedLinePipeline{};

    /**
     * Full refers to vertex format. Also refers to the lack of vertex color in the vertex data
     */
    PipelinePtr linePipeline{};
    PipelinePtr trianglePipeline{};

private:
    ResourceManager& resourceManager;

    DebugRendererCategory activeCategories{DebugRendererCategory::ALL};

    // for custom debug draws from Jolt. For primitives use the instanced draws instead.
    std::vector<DebugRendererVertexFull> lineVertices{};
    std::array<int64_t, FRAME_OVERLAP> lineVertexBufferSizes{0, 0};
    std::array<BufferPtr, FRAME_OVERLAP> lineVertexBuffers{};

    std::vector<DebugRendererVertexFull> triangleVertices{};
    std::array<int64_t, FRAME_OVERLAP> triangleVertexBufferSizes{0, 0};
    std::array<BufferPtr, FRAME_OVERLAP> triangleVertexBuffers{};

    DescriptorSetLayoutPtr uniformLayout{};

    std::vector<DebugRendererVertex> instancedVertices{};
    std::vector<uint32_t> instancedIndices{};
    BufferPtr instancedVertexBuffer{};
    BufferPtr instancedIndexBuffer{};

    /**
     * 0 = box, 1 = sphere
     */
    std::array<DebugRenderGroup, 4> debugRenderInstanceGroups;
};
}

#endif //DEBUG_RENDERER_H
