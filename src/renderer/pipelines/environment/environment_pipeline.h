//
// Created by William on 2025-01-25.
//

#ifndef ENVIRONMENT_PIPELINE_H
#define ENVIRONMENT_PIPELINE_H
#include <vulkan/vulkan_core.h>

#include "src/renderer/resource_manager.h"


namespace will_engine::environment_pipeline
{
struct EnvironmentDrawInfo
{
    bool bClearColor{false};
    VkImageView normalTarget{VK_NULL_HANDLE};
    VkImageView albedoTarget{VK_NULL_HANDLE};
    VkImageView pbrTarget{VK_NULL_HANDLE};
    VkImageView velocityTarget{VK_NULL_HANDLE};
    VkImageView depthTarget{VK_NULL_HANDLE};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset;
    VkDescriptorBufferBindingInfoEXT environmentMapBinding{};
    VkDeviceSize environmentMapOffset;
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

    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
};
}


#endif //ENVIRONMENT_PIPELINE_H
