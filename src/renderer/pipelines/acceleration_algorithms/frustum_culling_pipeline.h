//
// Created by William on 2024-12-07.
//

#ifndef FRUSTUM_CULL_PIPELINE_H
#define FRUSTUM_CULL_PIPELINE_H

#include <vector>
#include <vulkan/vulkan_core.h>


#include "src/renderer/vulkan_context.h"
#include "src/renderer/render_object/render_object.h"


struct FrustumCullDrawInfo;
struct FrustumCullPipelineCreateInfo;

class FrustumCullingPipeline
{
public:
    explicit FrustumCullingPipeline(VulkanContext& context);

    ~FrustumCullingPipeline();

    void init(const FrustumCullPipelineCreateInfo& createInfo);

    void draw(VkCommandBuffer cmd, const FrustumCullDrawInfo& drawInfo) const;

private:
    VulkanContext& context;
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};

    /**
     * Transient (Lifetime is managed outside of this pipeline)
     */
    VkDescriptorSetLayout sceneDataLayout{VK_NULL_HANDLE};
    /**
     * Transient (Lifetime is managed outside of this pipeline)
     * todo: move this to be lifetime owned by this pipeline
     */
    VkDescriptorSetLayout frustumCullLayout{VK_NULL_HANDLE};

    void cleanup();
};


#endif //FRUSTUM_CULL_PIPELINE_H
