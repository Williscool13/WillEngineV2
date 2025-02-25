//
// Created by William on 2025-02-24.
//

#ifndef TERRAIN_PIPELINE_H
#define TERRAIN_PIPELINE_H
#include "src/renderer/resource_manager.h"

struct TerrainDrawInfo
{};

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


#endif //TERRAIN_PIPELINE_H
