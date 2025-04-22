//
// Created by William on 2025-01-26.
//

#ifndef VISIBILITY_PASS_PIPELINE_H
#define VISIBILITY_PASS_PIPELINE_H

#include <volk/volk.h>

namespace will_engine
{
class ResourceManager;
class RenderObject;
}

namespace will_engine::visibility_pass_pipeline
{
struct VisibilityPassDrawInfo;


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
