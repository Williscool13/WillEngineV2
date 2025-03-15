//
// Created by William on 2025-01-26.
//

#ifndef CASCADED_SHADOW_MAP_H
#define CASCADED_SHADOW_MAP_H
#include "shadows.h"
#include "shadow_types.h"
#include "src/renderer/resource_manager.h"
#include "src/renderer/lighting/directional_light.h"
#include "src/renderer/assets/render_object/render_object.h"

namespace will_engine
{
namespace terrain
{
    class TerrainChunk;
}

class Camera;
}

namespace will_engine::cascaded_shadows
{
struct CascadeSplit
{
    float nearPlane;
    float farPlane;
    float padding[2];
};

struct CascadeShadowMapData
{
    int32_t cascadeLevel{-1};
    CascadeSplit split{};
    AllocatedImage depthShadowMap{VK_NULL_HANDLE};
    glm::mat4 lightViewProj{};
};

struct CascadedShadowMapGenerationPushConstants
{
    int32_t cascadeIndex{};
    int32_t tessLevel{};
};

struct CascadeShadowData
{
    CascadeSplit cascadeSplits[4];
    glm::mat4 lightViewProj[4];
    DirectionalLightData directionalLightData;
    float nearShadowPlane;
    float farShadowPlane;
    glm::vec2 pad;
};

class CascadedShadowMap
{
public:
    CascadedShadowMap() = delete;

    void createRenderObjectPipeline();

    void createTerrainPipeline();

    explicit CascadedShadowMap(ResourceManager& resourceManager);

    ~CascadedShadowMap();

    void update(const DirectionalLight& mainLight, const Camera* camera, int32_t currentFrameOverlap);

    void draw(VkCommandBuffer cmd, const std::vector<RenderObject*>& renderObjects, const std::vector<ITerrain*>& terrains, int32_t currentFrameOverlap);

    /**
     *
     * @param lightDirection
     * @param camera
     * @param cascadeNear
     * @param cascadeFar
     * @param reversedDepth If visualizing with a pipeline that expects reversed depth buffers, use this to reverse Zs in the Orthographic Projection matrix.
     * @return
     */
    static glm::mat4 getLightSpaceMatrix(glm::vec3 lightDirection, const Camera* camera, float cascadeNear, float cascadeFar, bool reversedDepth = false);

    VkDescriptorSetLayout getCascadedShadowMapUniformLayout() const { return cascadedShadowMapUniformLayout; }
    VkDescriptorSetLayout getCascadedShadowMapSamplerLayout() const { return cascadedShadowMapSamplerLayout; }
    const DescriptorBufferUniform& getCascadedShadowMapUniformBuffer() const { return cascadedShadowMapDescriptorBufferUniform; }
    const DescriptorBufferSampler& getCascadedShadowMapSamplerBuffer() const { return cascadedShadowMapDescriptorBufferSampler; }

    void reloadShaders()
    {
        createRenderObjectPipeline();
        createTerrainPipeline();
    }

public: // Debug

    AllocatedImage getShadowMap(const int32_t cascadeLevel) const
    {
        if (cascadeLevel >= shadows::SHADOW_CASCADE_COUNT || cascadeLevel < 0) {
            return shadowMaps[0].depthShadowMap;
        }
        return shadowMaps[cascadeLevel].depthShadowMap;
    }

    CascadeShadowMapData getCascadedShadowMapData(const int32_t cascadeLevel) const
    {
        if (cascadeLevel >= shadows::SHADOW_CASCADE_COUNT || cascadeLevel < 0) {
            return shadowMaps[0];
        }
        return shadowMaps[cascadeLevel];
    }

private:
    ResourceManager& resourceManager;

    CascadeShadowMapData shadowMaps[shadows::SHADOW_CASCADE_COUNT]{
        {0, {}, {VK_NULL_HANDLE}, {}},
        {1, {}, {VK_NULL_HANDLE}, {}},
        {2, {}, {VK_NULL_HANDLE}, {}},
        {3, {}, {VK_NULL_HANDLE}, {}},
    };

    VkDescriptorSetLayout cascadedShadowMapUniformLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout cascadedShadowMapSamplerLayout{VK_NULL_HANDLE};

    VkSampler sampler{VK_NULL_HANDLE};
    VkPipelineLayout renderObjectPipelineLayout{VK_NULL_HANDLE};
    VkPipeline renderObjectPipeline{VK_NULL_HANDLE};
    VkPipelineLayout terrainPipelineLayout{VK_NULL_HANDLE};
    VkPipeline terrainPipeline{VK_NULL_HANDLE};

    // contains the depth maps used by deferred resolve
    DescriptorBufferSampler cascadedShadowMapDescriptorBufferSampler;
    // contains the cascaded shadow map properties used by deferred resolve
    //AllocatedBuffer cascadedShadowMapData{VK_NULL_HANDLE};
    AllocatedBuffer cascadedShadowMapDatas[FRAME_OVERLAP]{VK_NULL_HANDLE};
    DescriptorBufferUniform cascadedShadowMapDescriptorBufferUniform;
};
}


#endif //CASCADED_SHADOW_MAP_H
