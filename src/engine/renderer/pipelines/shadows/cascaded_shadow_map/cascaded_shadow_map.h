//
// Created by William on 2025-01-26.
//

#ifndef CASCADED_SHADOW_MAP_H
#define CASCADED_SHADOW_MAP_H

#include <array>

#include "shadow_types.h"
#include "engine/renderer/renderer_constants.h"
#include "engine/renderer/lighting/directional_light.h"
#include "engine/renderer/resources/descriptor_set_layout.h"

namespace will_engine
{
namespace terrain
{
    class TerrainChunk;
}

class Camera;
}

namespace will_engine::renderer
{
class ResourceManager;


class CascadedShadowMap
{
public:
    CascadedShadowMap() = delete;

    void createRenderObjectPipeline();

    void createTerrainPipeline();

    explicit CascadedShadowMap(ResourceManager& resourceManager);

    ~CascadedShadowMap();

    void update(const DirectionalLight& mainLight, const Camera* camera, int32_t currentFrameOverlap);

    void draw(VkCommandBuffer cmd, CascadedShadowMapDrawInfo drawInfo);

    /**
     *
     * @param cascadeLevel
     * @param lightDirection
     * @param camera
     * @param cascadeNear
     * @param cascadeFar
     * @param reversedDepth If visualizing with a pipeline that expects reversed depth buffers, use this to reverse Zs in the Orthographic Projection matrix.
     * @return
     */
    glm::mat4 getLightSpaceMatrix(int32_t cascadeLevel, glm::vec3 lightDirection, const Camera* camera, float cascadeNear, float cascadeFar, bool reversedDepth = false) const;

    VkDescriptorSetLayout getCascadedShadowMapUniformLayout() const { return cascadedShadowMapUniformLayout->layout; }
    VkDescriptorSetLayout getCascadedShadowMapSamplerLayout() const { return cascadedShadowMapSamplerLayout->layout; }
    const DescriptorBufferUniform* getCascadedShadowMapUniformBuffer() const { return cascadedShadowMapDescriptorBufferUniform.get(); }
    const DescriptorBufferSampler* getCascadedShadowMapSamplerBuffer() const { return cascadedShadowMapDescriptorBufferSampler.get(); }

    void generateSplits();

    void reloadShaders()
    {
        createRenderObjectPipeline();
        createTerrainPipeline();
    }

    void setCascadedShadowMapProperties(const CascadedShadowMapSettings& csmProperties)
    {
        this->csmProperties = csmProperties;
    }

    CascadedShadowMapSettings getCascadedShadowMapProperties() const
    {
        return csmProperties;
    }

public: // Debug

    const ImageResource* getShadowMap(const int32_t cascadeLevel) const
    {
        if (cascadeLevel >= SHADOW_CASCADE_COUNT || cascadeLevel < 0) {
            return shadowMaps[0].depthShadowMap.get();
        }
        return shadowMaps[cascadeLevel].depthShadowMap.get();
    }

private:
    ResourceManager& resourceManager;

    std::array<CascadeShadowMapData, SHADOW_CASCADE_COUNT> shadowMaps{
        CascadeShadowMapData(0, {}, {}, {}),
        {1, {}, {}, {}},
        {2, {}, {}, {}},
        {3, {}, {}, {}},
    };

    DescriptorSetLayoutPtr cascadedShadowMapUniformLayout{};
    DescriptorSetLayoutPtr cascadedShadowMapSamplerLayout{};

    SamplerPtr sampler{};
    PipelineLayoutPtr renderObjectPipelineLayout{};
    PipelinePtr renderObjectPipeline{};
    PipelineLayoutPtr terrainPipelineLayout{};
    PipelinePtr terrainPipeline{};

    // contains the depth maps used by deferred resolve
    DescriptorBufferSamplerPtr cascadedShadowMapDescriptorBufferSampler;
    // contains the cascaded shadow map properties used by deferred resolve
    //Buffer cascadedShadowMapData{VK_NULL_HANDLE};
    BufferPtr cascadedShadowMapDatas[FRAME_OVERLAP]{};
    DescriptorBufferUniformPtr cascadedShadowMapDescriptorBufferUniform;

private:
    CascadedShadowMapSettings csmProperties{};
};
}


#endif //CASCADED_SHADOW_MAP_H
