//
// Created by William on 2024-12-07.
//

#ifndef DEFERRED_MRT_H
#define DEFERRED_MRT_H

#include <vulkan/vulkan_core.h>

#include "src/renderer/vk_descriptor_buffer.h"
#include "src/renderer/vulkan_context.h"
#include "src/renderer/render_object/render_object.h"

struct DeferredMrtPipelineCreateInfo
{
    VkDescriptorSetLayout sceneDataLayout;
    VkDescriptorSetLayout addressesLayout;
    VkDescriptorSetLayout textureLayout;
};

struct DeferredMrtDrawInfo
{
    std::vector<RenderObject*> renderObjects{};
    VkImageView normalTarget{VK_NULL_HANDLE};
    VkImageView albedoTarget{VK_NULL_HANDLE};
    VkImageView pbrTarget{VK_NULL_HANDLE};
    VkImageView velocityTarget{VK_NULL_HANDLE};
    VkImageView depthTarget{VK_NULL_HANDLE};
    VkExtent2D renderExtent{};
    glm::vec2 viewportRenderExtent{};
    const DescriptorBufferUniform& sceneData;

    explicit DeferredMrtDrawInfo(const DescriptorBufferUniform& sceneData) : sceneData(sceneData) {}
};

struct DeferredMrtPipelineRenderInfo
{
    VkFormat normalFormat;
    VkFormat albedoFormat;
    VkFormat pbrFormat;
    VkFormat velocityFormat;
    VkFormat depthFormat;
};

class DeferredMrtPipeline
{
public:
    explicit DeferredMrtPipeline(VulkanContext& context);

    ~DeferredMrtPipeline();

    void init(const DeferredMrtPipelineCreateInfo& createInfo, const DeferredMrtPipelineRenderInfo& renderInfo);

    void draw(VkCommandBuffer cmd, DeferredMrtDrawInfo& drawInfo) const;

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
     */
    VkDescriptorSetLayout addressesLayout{VK_NULL_HANDLE};
    /**
     * Transient (Lifetime is managed outside of this pipeline)
     */
    VkDescriptorSetLayout textureLayout{VK_NULL_HANDLE};

    DeferredMrtPipelineRenderInfo renderFormats{};

    void createPipelineLayout();

    void createPipeline();

    void cleanup();
};


#endif //DEFERRED_MRT_H
