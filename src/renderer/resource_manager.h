//
// Created by William on 2024-12-13.
//

#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H
#include <filesystem>

#include "vk_descriptors.h"
#include "vk_pipelines.h"
#include "vk_types.h"
#include "descriptor_buffer/descriptor_buffer_sampler.h"
#include "descriptor_buffer/descriptor_buffer_uniform.h"


class ImmediateSubmitter;
class VulkanContext;

class ResourceManager
{
public:
    ResourceManager() = delete;

    ResourceManager(const VulkanContext& context, ImmediateSubmitter& immediate);

    ~ResourceManager();

    [[nodiscard]] AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) const;

    AllocatedBuffer createHostSequentialBuffer(size_t allocSize) const;

    AllocatedBuffer createDeviceBuffer(size_t allocSize, VkBufferUsageFlags additionalUsages = 0) const;

    [[nodiscard]] AllocatedBuffer createStagingBuffer(size_t allocSize) const;

    [[nodiscard]] AllocatedBuffer createReceivingBuffer(size_t allocSize) const;

    void copyBuffer(const AllocatedBuffer& src, const AllocatedBuffer& dst, VkDeviceSize size) const;

    void copyBuffer(const AllocatedBuffer& src, const AllocatedBuffer& dst, VkDeviceSize size, VkDeviceSize offset) const;

    [[nodiscard]] VkDeviceAddress getBufferAddress(const AllocatedBuffer& buffer) const;

    void destroyBuffer(AllocatedBuffer& buffer) const;


    [[nodiscard]] VkSampler createSampler(const VkSamplerCreateInfo& createInfo) const;

    void destroySampler(const VkSampler& sampler) const;


    [[nodiscard]] AllocatedImage createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false) const;

    [[nodiscard]] AllocatedImage createImage(const void* data, size_t dataSize, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false) const;

    [[nodiscard]] AllocatedImage createCubemap(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false) const;

    void destroyImage(const AllocatedImage& img) const;


    [[nodiscard]] DescriptorBufferSampler createDescriptorBufferSampler(VkDescriptorSetLayout layout, int32_t maxObjectCount) const;

    int32_t setupDescriptorBufferSampler(DescriptorBufferSampler& descriptorBuffer, const std::vector<will_engine::DescriptorImageData>& imageBuffers, int index = -1) const;

    [[nodiscard]] DescriptorBufferUniform createDescriptorBufferUniform(VkDescriptorSetLayout layout, int32_t maxObjectCount) const;

    int32_t setupDescriptorBufferUniform(DescriptorBufferUniform& descriptorBuffer, const std::vector<will_engine::DescriptorUniformData>& uniformBuffers, int index = -1) const;

    void destroyDescriptorBuffer(DescriptorBuffer& descriptorBuffer) const;


    VkShaderModule createShaderModule(const std::filesystem::path& path) const;

    void destroyShaderModule(VkShaderModule& shaderModule) const;


    VkPipelineLayout createPipelineLayout(const VkPipelineLayoutCreateInfo& createInfo) const;

    void destroyPipelineLayout(VkPipelineLayout& pipelineLayout) const;

    VkPipeline createRenderPipeline(PipelineBuilder& builder) const;

    VkPipeline createComputePipeline(const VkComputePipelineCreateInfo& pipelineInfo) const;

    void destroyPipeline(VkPipeline& pipeline) const;

    VkDescriptorSetLayout createDescriptorSetLayout(DescriptorLayoutBuilder& layoutBuilder, VkShaderStageFlagBits shaderStageFlags, VkDescriptorSetLayoutCreateFlagBits layoutCreateFlags) const;

    void destroyDescriptorSetLayout(VkDescriptorSetLayout& descriptorSetLayout) const;

    VkImageView createImageView(const VkImageViewCreateInfo& viewInfo) const;

    void destroyImageView(VkImageView& imageView) const;

public:
    [[nodiscard]] VkSampler getDefaultSamplerLinear() const { return defaultSamplerLinear; }
    [[nodiscard]] VkSampler getDefaultSamplerNearest() const { return defaultSamplerNearest; }
    [[nodiscard]] AllocatedImage getWhiteImage() const { return whiteImage; }
    [[nodiscard]] AllocatedImage getErrorCheckerboardImage() const { return errorCheckerboardImage; }

    [[nodiscard]] VkDescriptorSetLayout getEmptyLayout() const { return emptyDescriptorSetLayout; }
    [[nodiscard]] VkDescriptorSetLayout getSceneDataLayout() const { return sceneDataLayout; }
    [[nodiscard]] VkDescriptorSetLayout getFrustumCullLayout() const { return frustumCullLayout; }
    [[nodiscard]] VkDescriptorSetLayout getAddressesLayout() const { return addressesLayout; }
    [[nodiscard]] VkDescriptorSetLayout getTexturesLayout() const { return texturesLayout; }
    [[nodiscard]] VkDescriptorSetLayout getRenderTargetsLayout() const { return renderTargetsLayout; }

private:
    const VulkanContext& context;
    const ImmediateSubmitter& immediate;

    AllocatedImage whiteImage{};
    AllocatedImage errorCheckerboardImage{};
    VkSampler defaultSamplerLinear{VK_NULL_HANDLE};
    VkSampler defaultSamplerNearest{VK_NULL_HANDLE};

    VkDescriptorSetLayout emptyDescriptorSetLayout{VK_NULL_HANDLE};

    VkDescriptorSetLayout sceneDataLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout frustumCullLayout{VK_NULL_HANDLE};
    /**
     * Material and Instance Buffer Addresses
     */
    VkDescriptorSetLayout addressesLayout{VK_NULL_HANDLE};
    /**
     * Sampler and Image Arrays
     */
    VkDescriptorSetLayout texturesLayout{VK_NULL_HANDLE};

    /**
     * Used in deferred resolve
     */
    VkDescriptorSetLayout renderTargetsLayout{VK_NULL_HANDLE};
};

#endif //RESOURCE_MANAGER_H
