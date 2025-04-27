//
// Created by William on 2025-04-27.
//

#ifndef DEBUG_RENDERER_H
#define DEBUG_RENDERER_H
#include "debug_renderer_types.h"
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



    DebugRenderer(ResourceManager& resourceManager);

    ~DebugRenderer();

    void drawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color);

    void drawSphere(const glm::vec3& center, float radius, const glm::vec3& color, DebugRendererCategory category = DebugRendererCategory::General);

    void drawBox(const glm::vec3& center, const glm::vec3& dimensions, const glm::vec3& color, DebugRendererCategory category = DebugRendererCategory::General);

    void drawBoxMinmax(const glm::vec3& min, const glm::vec3& max, const glm::vec3& color, DebugRendererCategory category = DebugRendererCategory::General);

    void render(VkCommandBuffer cmd);

    void clear();

    void reloadShaders()
    {
        createPipeline();
    }

private:
    void createPipeline();

    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

private:
    ResourceManager& resourceManager;
    void generateBuffers();

    DebugRendererCategory activeCategories{DebugRendererCategory::ALL};

    // for custom debug draws from Jolt. For primitives use the instanced draws instead.
    std::vector<DebugRendererVertex> vertices{};
    std::vector<uint32_t> indices{};
    AllocatedBuffer vertexBuffer{VK_NULL_HANDLE};
    AllocatedBuffer indexBuffer{VK_NULL_HANDLE};


    VkDescriptorSetLayout addressesLayout;

    std::vector<DebugRendererVertex> instancedVertices;
    std::vector<uint32_t> instancedIndices;
    AllocatedBuffer instancedVertexBuffer;
    AllocatedBuffer instancedIndexBuffer;

    /**
     * Default size is 512 instances
     */
    std::vector<BoxInstance> boxInstances;
    AllocatedBuffer boxInstanceBuffer{VK_NULL_HANDLE};

    /**
     * Includes:
     * \n \code{ boxInstanceBuffer.address }\endcode
     */
    AllocatedBuffer addressBuffer{VK_NULL_HANDLE};
    /**
     * Should only house 1x address buffer, but for "correctness" should use `FRAME_OVERLAP` number of address buffers.
     * \n But since addresses don't change often (almost never), will forgo this
     */
    DescriptorBufferUniform addressesDescriptorBuffer;

    VkDrawIndexedIndirectCommand boxDrawIndirect;
    AllocatedBuffer drawIndirectBuffer{VK_NULL_HANDLE};
};
}

#endif //DEBUG_RENDERER_H
