//
// Created by William on 2025-04-27.
//

#ifndef DEBUG_RENDERER_H
#define DEBUG_RENDERER_H
#include <array>

#include "debug_renderer_types.h"
#include "src/renderer/renderer_constants.h"
#include "src/renderer/vk_types.h"
#include "src/renderer/descriptor_buffer/descriptor_buffer_uniform.h"

namespace will_engine
{
class ResourceManager;
}

namespace will_engine::debug_renderer
{
class DebugRenderer
{
public:
    static DebugRenderer* debugRenderer;
    static DebugRenderer* get() { return debugRenderer; }
    static void set(DebugRenderer* _debugRenderer) { debugRenderer = _debugRenderer; }


    void setupBoxRendering(int32_t index);

    void setupSphereRendering(int32_t index);

    DebugRenderer(ResourceManager& resourceManager);

    ~DebugRenderer();

public:
    static void drawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color);

    static void drawSphere(const glm::vec3& center, float radius, const glm::vec3& color, DebugRendererCategory category = DebugRendererCategory::General);

    static void drawBox(const glm::vec3& center, const glm::vec3& dimensions, const glm::vec3& color, DebugRendererCategory category = DebugRendererCategory::General);

    static void drawBoxMinMax(const glm::vec3& min, const glm::vec3& max, const glm::vec3& color, DebugRendererCategory category = DebugRendererCategory::General);

private:
    void drawLineImpl(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color);

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

    VkPipeline pipeline{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};

private:
    ResourceManager& resourceManager;
    void generateBuffers();

    DebugRendererCategory activeCategories{DebugRendererCategory::ALL};

    // for custom debug draws from Jolt. For primitives use the instanced draws instead.
    // std::vector<DebugRendererVertex> vertices{};
    // std::vector<uint32_t> indices{};
    // AllocatedBuffer vertexBuffer{VK_NULL_HANDLE};
    // AllocatedBuffer indexBuffer{VK_NULL_HANDLE};


    VkDescriptorSetLayout uniformLayout{VK_NULL_HANDLE};

    std::vector<DebugRendererVertex> instancedVertices{};
    std::vector<uint32_t> instancedIndices{};
    AllocatedBuffer instancedVertexBuffer{VK_NULL_HANDLE};
    AllocatedBuffer instancedIndexBuffer{VK_NULL_HANDLE};

    /**
     * 0 = box, 1 = sphere
     */
    std::array<DebugRenderGroup, 4> debugRenderInstanceGroups;




};
}

#endif //DEBUG_RENDERER_H
