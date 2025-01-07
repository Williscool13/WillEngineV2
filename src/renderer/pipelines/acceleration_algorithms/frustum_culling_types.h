//
// Created by William on 2025-01-07.
//

#ifndef FRUSTUM_CULLING_TYPES_H
#define FRUSTUM_CULLING_TYPES_H

struct FrustumCullingPushConstants
{
    int32_t enable{};
};

struct FrustumCullPipelineCreateInfo
{
    VkDescriptorSetLayout sceneDataLayout;
    VkDescriptorSetLayout frustumCullLayout;
};

struct FrustumCullDrawInfo
{
    std::vector<RenderObject*> renderObjects;
    const DescriptorBufferUniform& sceneData;
    int32_t currentFrameOverlap;
    bool enableFrustumCulling;
};

#endif //FRUSTUM_CULLING_TYPES_H
