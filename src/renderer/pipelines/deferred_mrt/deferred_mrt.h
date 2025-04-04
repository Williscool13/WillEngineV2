//
// Created by William on 2025-01-24.
//

#ifndef DEFERRED_MRT_H
#define DEFERRED_MRT_H

#include <vulkan/vulkan_core.h>

#include "src/renderer/resource_manager.h"
#include "src/renderer/assets/render_object/render_object.h"

namespace will_engine::deferred_mrt
{
struct DeferredMrtDrawInfo
{
    bool bClearColor{true};
    int32_t currentFrameOverlap{0};
    glm::vec2 viewportExtents{RENDER_EXTENT_WIDTH, RENDER_EXTENT_HEIGHT};
    const std::vector<RenderObject*>& renderObjects{};
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

    void reloadShaders() { createPipeline(); }

private:
    void createPipeline();

private:
    ResourceManager& resourceManager;

    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
};
}


#endif //DEFERRED_MRT_H
