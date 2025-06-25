//
// Created by William on 2025-01-25.
//

#ifndef ENVIRONMENT_PIPELINE_H
#define ENVIRONMENT_PIPELINE_H

#include <volk/volk.h>

#include "engine/renderer/renderer_constants.h"
#include "engine/renderer/resources/resources_fwd.h"

namespace will_engine::renderer
{
class ResourceManager;

struct EnvironmentDrawInfo
{
    VkExtent2D renderExtents{DEFAULT_RENDER_EXTENT_2D};
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

    PipelineLayoutPtr pipelineLayout{};
    PipelinePtr pipeline{};
};
}


#endif //ENVIRONMENT_PIPELINE_H
