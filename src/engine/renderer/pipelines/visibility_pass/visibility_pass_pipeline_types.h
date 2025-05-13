//
// Created by William on 2025-04-14.
//

#ifndef VISIBILITY_PASS_PIPELINE_TYPES_H
#define VISIBILITY_PASS_PIPELINE_TYPES_H

#include <vector>
#include <volk/volk.h>

namespace will_engine
{
class RenderObject;
}

namespace will_engine::visibility_pass_pipeline
{
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
}

#endif //VISIBILITY_PASS_PIPELINE_TYPES_H
