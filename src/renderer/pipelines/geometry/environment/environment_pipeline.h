//
// Created by William on 2025-01-25.
//

#ifndef ENVIRONMENT_PIPELINE_H
#define ENVIRONMENT_PIPELINE_H

#include <volk/volk.h>

namespace will_engine
{
class ResourceManager;
}

namespace will_engine::environment_pipeline
{
struct EnvironmentDrawInfo;

class EnvironmentPipeline
{
public:
    EnvironmentPipeline(ResourceManager& resourceManager, VkDescriptorSetLayout environmentMapLayout);

    ~EnvironmentPipeline();

    void draw(VkCommandBuffer cmd, const EnvironmentDrawInfo& drawInfo) const;

    void reloadShaders() { createPipeline(); }

private:
    void createPipeline();

private:
    ResourceManager& resourceManager;

    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
};
}


#endif //ENVIRONMENT_PIPELINE_H
