//
// Created by William on 2025-01-26.
//

#ifndef VISIBILITY_PASS_PIPELINE_H
#define VISIBILITY_PASS_PIPELINE_H
#include "src/renderer/resource_manager.h"


namespace will_engine
{
class RenderObject;
}

namespace will_engine::visibility_pass
{
struct VisibilityPassPushConstants
{
    int32_t enable{};
    int32_t shadowPass{};
};

struct VisibilityPassDrawInfo
{
    int32_t currentFrameOverlap{0};
    std::vector<RenderObject*> renderObjects{};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
    bool bEnableFrustumCulling{};
    bool bIsShadowPass{};
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

    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
};
}


#endif //VISIBILITY_PASS_PIPELINE_H
