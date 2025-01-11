//
// Created by William on 2024-12-07.
//

#ifndef DEFERRED_MRT_H
#define DEFERRED_MRT_H

#include <vulkan/vulkan_core.h>

#include "src/renderer/vk_descriptor_buffer.h"

class RenderObject;

struct DeferredMrtPipelineCreateInfo
{
    VkDescriptorSetLayout sceneDataLayout;
    VkDescriptorSetLayout modelAddressesLayout;
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
    VkDescriptorSetLayout modelAddressesLayout{VK_NULL_HANDLE};
    /**
     * Transient (Lifetime is managed outside of this pipeline)
     */
    VkDescriptorSetLayout textureLayout{VK_NULL_HANDLE};

    DeferredMrtPipelineRenderInfo renderFormats{};
};


#endif //DEFERRED_MRT_H
