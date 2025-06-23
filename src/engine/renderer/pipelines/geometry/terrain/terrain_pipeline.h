//
// Created by William on 2025-02-24.
//

#ifndef TERRAIN_PIPELINE_H
#define TERRAIN_PIPELINE_H

#include <unordered_set>
#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>

#include "engine/renderer/renderer_constants.h"
#include "engine/renderer/resources/resources_fwd.h"

namespace will_engine
{
class ITerrain;
}

namespace will_engine::renderer
{
class ResourceManager;

struct TerrainPushConstants
{
    float tessLevel;
};

struct TerrainDrawInfo
{
    bool bClearColor{true};
    int32_t currentFrameOverlap{0};
    VkExtent2D renderExtents{DEFAULT_RENDER_EXTENTS_2D};
    const std::unordered_set<ITerrain*>& terrains;
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

    void draw(VkCommandBuffer cmd, const TerrainDrawInfo& drawInfo) const;

    void reloadShaders()
    {
        createPipeline();
    }

private:
    void createPipeline();

private:
    ResourceManager& resourceManager;

    PipelineLayoutPtr pipelineLayout{};
    PipelinePtr pipeline{};

    DescriptorSetLayoutPtr descriptorSetLayout{};
    DescriptorBufferSamplerPtr descriptorBuffer;
};
}


#endif //TERRAIN_PIPELINE_H
