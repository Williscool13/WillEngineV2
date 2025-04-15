//
// Created by William on 2025-02-24.
//

#ifndef TERRAIN_PIPELINE_H
#define TERRAIN_PIPELINE_H

#include <volk/volk.h>

#include "src/renderer/descriptor_buffer/descriptor_buffer_sampler.h"

namespace will_engine
{
class ResourceManager;
}

namespace will_engine::terrain
{
struct TerrainDrawInfo;
class TerrainChunk;

class TerrainPipeline
{
public:
    explicit TerrainPipeline(ResourceManager& resourceManager);

    ~TerrainPipeline();

    void draw(VkCommandBuffer cmd, const TerrainDrawInfo& drawInfo) const;

    void reloadShaders()
    {
        createPipeline();
        createLinePipeline();
    }

private:
    void createPipeline();

    void createLinePipeline();

private:
    ResourceManager& resourceManager;

    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};

    VkPipeline linePipeline{VK_NULL_HANDLE};

    VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
    DescriptorBufferSampler descriptorBuffer;
};
}


#endif //TERRAIN_PIPELINE_H
