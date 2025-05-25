//
// Created by William on 2025-01-24.
//

#ifndef DEFERRED_MRT_H
#define DEFERRED_MRT_H

#include <volk/volk.h>
#include <glm/glm.hpp>

#include "engine/renderer/renderer_constants.h"
#include "engine/renderer/resources/pipeline.h"
#include "engine/renderer/resources/pipeline_layout.h"

namespace will_engine::renderer
{
class RenderObject;
class ResourceManager;

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

    PipelineLayout pipelineLayout{};
    Pipeline pipeline{};
};
}


#endif //DEFERRED_MRT_H
