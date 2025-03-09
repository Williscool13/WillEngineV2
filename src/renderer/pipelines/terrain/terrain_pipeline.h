//
// Created by William on 2025-02-24.
//

#ifndef TERRAIN_PIPELINE_H
#define TERRAIN_PIPELINE_H
#include "src/core/game_object/terrain.h"
#include "src/renderer/renderer_constants.h"
#include "src/renderer/resource_manager.h"


namespace will_engine::terrain
{
class TerrainChunk;

struct TerrainDrawInfo
{
    bool bClearColor{true};
    int32_t currentFrameOverlap{0};
    glm::vec2 viewportExtents{RENDER_EXTENT_WIDTH, RENDER_EXTENT_HEIGHT};
    const std::vector<ITerrain*>& terrains;
    VkImageView normalTarget{VK_NULL_HANDLE};
    VkImageView albedoTarget{VK_NULL_HANDLE};
    VkImageView pbrTarget{VK_NULL_HANDLE};
    VkImageView velocityTarget{VK_NULL_HANDLE};
    VkImageView depthTarget{VK_NULL_HANDLE};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
};

class TerrainPipeline
{
public:
    explicit TerrainPipeline(ResourceManager& resourceManager);

    ~TerrainPipeline();

    //void setupDescriptorBuffer(const TerrainDescriptor& descriptor);

    void draw(VkCommandBuffer cmd, const TerrainDrawInfo& drawInfo) const;

    void reloadShaders() { createPipeline(); }

private:
    void createPipeline();

private:
    ResourceManager& resourceManager;

    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
    VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
    DescriptorBufferSampler descriptorBuffer;
};
}


#endif //TERRAIN_PIPELINE_H
