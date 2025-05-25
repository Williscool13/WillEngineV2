//
// Created by William on 2025-01-26.
//

#ifndef VISIBILITY_PASS_PIPELINE_H
#define VISIBILITY_PASS_PIPELINE_H

#include <vector>
#include <volk/volk.h>

#include "engine/renderer/resources/pipeline.h"
#include "engine/renderer/resources/pipeline_layout.h"

namespace will_engine::renderer
{
class ResourceManager;
class RenderObject;

struct VisibilityPassPushConstants
{
    int32_t enable{};
    int32_t shadowPass{};
};

struct VisibilityPassDrawInfo
{
    int32_t currentFrameOverlap{0};
    const std::vector<RenderObject*>& renderObjects{};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
    bool bEnableFrustumCulling{};
    bool bIsShadowPass{};
    bool bIsOpaque{};
};


class VisibilityPassPipeline
{
public:
    explicit VisibilityPassPipeline(ResourceManager& resourceManager);

    ~VisibilityPassPipeline();

    void draw(VkCommandBuffer cmd, const VisibilityPassDrawInfo& drawInfo) const;

    void reloadShaders() { createPipeline(); }

private:
    void createPipeline();

private:
    ResourceManager& resourceManager;

    PipelineLayout pipelineLayout{};
    Pipeline pipeline{};
};
}


#endif //VISIBILITY_PASS_PIPELINE_H
