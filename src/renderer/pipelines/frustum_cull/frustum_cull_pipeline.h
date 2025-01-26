//
// Created by William on 2025-01-26.
//

#ifndef FRUSTUM_CULL_PIPELINE_H
#define FRUSTUM_CULL_PIPELINE_H
#include "src/renderer/resource_manager.h"


namespace will_engine
{
class RenderObject;
}

namespace will_engine::frustum_cull_pipeline
{
struct FrustumCullingPushConstants
{
    int32_t enable{};
};

struct FrustumCullDrawInfo
{
    std::vector<RenderObject*> renderObjects;
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
    bool enableFrustumCulling;
};

class FrustumCullPipeline {
public:
    explicit FrustumCullPipeline(ResourceManager& resourceManager);

    ~FrustumCullPipeline();

    void draw(VkCommandBuffer cmd, const FrustumCullDrawInfo& drawInfo) const;

private:
    ResourceManager& resourceManager;

    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
};

}



#endif //FRUSTUM_CULL_PIPELINE_H
