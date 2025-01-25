//
// Created by William on 2025-01-24.
//

#ifndef DEFERRED_MRT_H
#define DEFERRED_MRT_H

#include <vulkan/vulkan_core.h>

#include "src/renderer/render_object/render_object.h"

class ResourceManager;

namespace will_engine::deferred_mrt
{

struct DeferredMrtPushConstants
{
    glm::mat4 testMatrix;
};

struct DeferredMrtDrawInfo
{
    std::vector<RenderObject*> renderObjects{};
    VkImageView normalTarget{VK_NULL_HANDLE};
    VkImageView albedoTarget{VK_NULL_HANDLE};
    VkImageView pbrTarget{VK_NULL_HANDLE};
    VkImageView velocityTarget{VK_NULL_HANDLE};
    VkImageView depthTarget{VK_NULL_HANDLE};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
};

class DeferredMrtPipeline
{
public:
    explicit DeferredMrtPipeline(ResourceManager& resourceManager);

    ~DeferredMrtPipeline();

    void draw(VkCommandBuffer cmd, const DeferredMrtDrawInfo& drawInfo) const;

private:
    ResourceManager& resourceManager;

    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
};
}


#endif //DEFERRED_MRT_H
