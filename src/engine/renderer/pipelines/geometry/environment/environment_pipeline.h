//
// Created by William on 2025-01-25.
//

#ifndef ENVIRONMENT_PIPELINE_H
#define ENVIRONMENT_PIPELINE_H

#include <volk/volk.h>

#include "engine/renderer/resources/pipeline.h"
#include "engine/renderer/resources/pipeline_layout.h"

namespace will_engine::renderer
{
class ResourceManager;

struct EnvironmentDrawInfo
{
    bool bClearColor{false};
    VkImageView normalTarget{VK_NULL_HANDLE};
    VkImageView albedoTarget{VK_NULL_HANDLE};
    VkImageView pbrTarget{VK_NULL_HANDLE};
    VkImageView velocityTarget{VK_NULL_HANDLE};
    VkImageView depthTarget{VK_NULL_HANDLE};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{};
    VkDescriptorBufferBindingInfoEXT environmentMapBinding{};
    VkDeviceSize environmentMapOffset{};
};

class EnvironmentPipeline
{
public:
    EnvironmentPipeline(ResourceManager& resourceManager, VkDescriptorSetLayout environmentMapLayout);

    ~EnvironmentPipeline();

    void draw(VkCommandBuffer cmd, const EnvironmentDrawInfo& drawInfo) const;

    void reloadShaders() { createPipeline(); }

private:
    void createPipeline();

private:
    ResourceManager& resourceManager;

    PipelineLayout pipelineLayout{};
    Pipeline pipeline{};
};
}


#endif //ENVIRONMENT_PIPELINE_H
