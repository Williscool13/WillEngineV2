//
// Created by William on 2025-01-25.
//

#ifndef POST_PROCESS_PIPELINE_H
#define POST_PROCESS_PIPELINE_H
#include "src/renderer/resource_manager.h"
#include "src/renderer/post_process/post_process_types.h"


namespace will_engine::post_process_pipeline
{
struct PostProcessDescriptor
{
    VkImageView inputImage;
    VkImageView outputImage;
    VkSampler sampler;
};

struct PostProcessPushConstants
{
    int32_t width;
    int32_t height;
    uint32_t postProcessFlags;
    int32_t padding;
};

class PostProcessPipeline
{
public:
    explicit PostProcessPipeline(ResourceManager& resourceManager);

    ~PostProcessPipeline();

    void setupDescriptorBuffer(const PostProcessDescriptor& bufferInfo);

    void draw(VkCommandBuffer cmd, post_process::PostProcessType postProcessFlags) const;

    void reloadShaders() { createPipeline(); }

private:
    void createPipeline();

private:
    ResourceManager& resourceManager;

    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
    VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
    DescriptorBufferSampler descriptorBuffer;
};
}


#endif //POST_PROCESS_PIPELINE_H
