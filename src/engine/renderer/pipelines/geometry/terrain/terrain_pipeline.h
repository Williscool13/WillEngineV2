//
// Created by William on 2025-02-24.
//

#ifndef TERRAIN_PIPELINE_H
#define TERRAIN_PIPELINE_H

#include <unordered_set>
#include <volk/volk.h>
#include <glm/glm.hpp>

#include "engine/renderer/renderer_constants.h"
#include "engine/renderer/resources/descriptor_set_layout.h"
#include "engine/renderer/resources/pipeline.h"
#include "engine/renderer/resources/pipeline_layout.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_sampler.h"

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
    glm::vec2 viewportExtents{RENDER_EXTENT_WIDTH, RENDER_EXTENT_HEIGHT};
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

    PipelineLayout pipelineLayout{};
    Pipeline pipeline{};

    DescriptorSetLayout descriptorSetLayout{};
    DescriptorBufferSampler descriptorBuffer;
};
}


#endif //TERRAIN_PIPELINE_H
