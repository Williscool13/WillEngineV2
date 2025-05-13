//
// Created by William on 2025-01-24.
//

#ifndef DEFERRED_MRT_H
#define DEFERRED_MRT_H

#include <volk/volk.h>

namespace will_engine
{
class ResourceManager;
class RenderObject;
}

namespace will_engine::deferred_mrt
{
struct DeferredMrtDrawInfo;

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
