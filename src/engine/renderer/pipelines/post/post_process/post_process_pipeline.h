//
// Created by William on 2025-01-25.
//

#ifndef POST_PROCESS_PIPELINE_H
#define POST_PROCESS_PIPELINE_H

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

#include "post_process_pipeline_types.h"
#include "engine/renderer/renderer_constants.h"
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
    uint32_t postProcessFlags;
    glm::vec3 padding;
};

struct PostProcessDrawInfo
{
    PostProcessType postProcessFlags{PostProcessType::ALL};
    VkExtent2D extents{DEFAULT_RENDER_EXTENT_2D};
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
