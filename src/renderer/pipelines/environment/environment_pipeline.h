//
// Created by William on 2025-01-25.
//

#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H
#include <vulkan/vulkan_core.h>


class ResourceManager;

namespace will_engine::environment
{
struct EnvironmentDrawInfo
{
    VkImageView colorAttachment;
    VkImageView depthAttachment;
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset;
    VkDescriptorBufferBindingInfoEXT environmentMapBinding{};
    VkDeviceSize environmentMapOffset;
};


class EnvironmentPipeline
{
public:
    EnvironmentPipeline(ResourceManager* _resourceManager, VkDescriptorSetLayout environmentMapLayout);

    ~EnvironmentPipeline();

    void draw(VkCommandBuffer cmd, const EnvironmentDrawInfo& drawInfo) const;

private:
    ResourceManager* resourceManager{nullptr};

    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
};
}


#endif //ENVIRONMENT_H
