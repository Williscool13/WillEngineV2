//
// Created by William on 2025-01-25.
//

#ifndef POST_PROCESS_PIPELINE_H
#define POST_PROCESS_PIPELINE_H

#include <vulkan/vulkan_core.h>

#include "post_process_pipeline_types.h"
#include "engine/renderer/resources/resources_fwd.h"

namespace will_engine::renderer
{
class ResourceManager;

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

struct PostProcessDrawInfo
{
    PostProcessType postProcessFlags{PostProcessType::ALL};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
};

class PostProcessPipeline
{
public:
    explicit PostProcessPipeline(ResourceManager& resourceManager);

    ~PostProcessPipeline();

    void setupDescriptorBuffer(const PostProcessDescriptor& bufferInfo);

    void draw(VkCommandBuffer cmd, PostProcessDrawInfo drawInfo) const;

    void reloadShaders() { createPipeline(); }

private:
    void createPipeline();

private:
    ResourceManager& resourceManager;

    PipelineLayoutPtr pipelineLayout{};
    PipelinePtr pipeline{};
    DescriptorSetLayoutPtr descriptorSetLayout{};
    DescriptorBufferSamplerPtr descriptorBuffer{};
};
}


#endif //POST_PROCESS_PIPELINE_H
