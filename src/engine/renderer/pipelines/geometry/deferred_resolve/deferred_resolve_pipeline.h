//
// Created by William on 2025-01-25.
//

#ifndef DEFERRED_RESOLVE_H
#define DEFERRED_RESOLVE_H

#include <volk/volk.h>

#include "engine/renderer/descriptor_buffer/descriptor_buffer_sampler.h"

namespace will_engine
{
class ResourceManager;
class DescriptorBuffer;
class DescriptorBufferSampler;
}

namespace will_engine::deferred_resolve
{
struct DeferredResolveDrawInfo;
struct DeferredResolveDescriptor;

class DeferredResolvePipeline
{
public:
    explicit DeferredResolvePipeline(ResourceManager& resourceManager, VkDescriptorSetLayout environmentIBLLayout,
                                     VkDescriptorSetLayout cascadeUniformLayout,
                                     VkDescriptorSetLayout cascadeSamplerLayout);

    ~DeferredResolvePipeline();

    void setupDescriptorBuffer(const DeferredResolveDescriptor& drawInfo);

    void draw(VkCommandBuffer cmd, const DeferredResolveDrawInfo& drawInfo) const;

    void reloadShaders() { createPipeline(); }

private:
    void createPipeline();

private:
    ResourceManager& resourceManager;

    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};

    DescriptorBufferSampler resolveDescriptorBuffer;
};
}


#endif //DEFERRED_RESOLVE_H
