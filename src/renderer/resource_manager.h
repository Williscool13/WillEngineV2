//
// Created by William on 2024-12-13.
//

#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H
#include <filesystem>
#include <fstream>

#include "vk_descriptors.h"
#include "vk_pipelines.h"
#include "vk_types.h"
#include "descriptor_buffer/descriptor_buffer_sampler.h"
#include "descriptor_buffer/descriptor_buffer_uniform.h"
#include "shaderc/shaderc.h"
#include "shaderc/shaderc.hpp"


class VulkanContext;

namespace will_engine
{
class ImmediateSubmitter;

class ResourceManager
{
public:
    ResourceManager() = delete;

    ResourceManager(const VulkanContext& context, ImmediateSubmitter& immediate);

    ~ResourceManager();

public: // Resource Creation
    [[nodiscard]] AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) const;

    AllocatedBuffer createHostSequentialBuffer(size_t allocSize) const;

    AllocatedBuffer createHostRandomBuffer(size_t allocSize, VkBufferUsageFlags additionalUsages = 0) const;

    AllocatedBuffer createDeviceBuffer(size_t allocSize, VkBufferUsageFlags additionalUsages = 0) const;

    [[nodiscard]] AllocatedBuffer createStagingBuffer(size_t allocSize) const;

    [[nodiscard]] AllocatedBuffer createReceivingBuffer(size_t allocSize) const;

    void copyBuffer(const AllocatedBuffer& src, const AllocatedBuffer& dst, VkDeviceSize size) const;

    void copyBuffer(const AllocatedBuffer& src, const AllocatedBuffer& dst, VkDeviceSize size, VkDeviceSize offset) const;

    [[nodiscard]] VkDeviceAddress getBufferAddress(const AllocatedBuffer& buffer) const;

    void destroyBuffer(AllocatedBuffer& buffer) const;


    [[nodiscard]] VkSampler createSampler(const VkSamplerCreateInfo& createInfo) const;

    void destroySampler(const VkSampler& sampler) const;


    [[nodiscard]] AllocatedImage createImage(const VkImageCreateInfo& createInfo) const;

    [[nodiscard]] AllocatedImage createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false) const;

    [[nodiscard]] AllocatedImage createImage(const void* data, size_t dataSize, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false) const;

    [[nodiscard]] AllocatedImage createCubemap(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false) const;

    void destroyImage(AllocatedImage& img) const;


    [[nodiscard]] DescriptorBufferSampler createDescriptorBufferSampler(VkDescriptorSetLayout layout, int32_t maxObjectCount) const;

    int32_t setupDescriptorBufferSampler(DescriptorBufferSampler& descriptorBuffer, const std::vector<will_engine::DescriptorImageData>& imageBuffers, int index = -1) const;

    [[nodiscard]] DescriptorBufferUniform createDescriptorBufferUniform(VkDescriptorSetLayout layout, int32_t maxObjectCount) const;

    int32_t setupDescriptorBufferUniform(DescriptorBufferUniform& descriptorBuffer, const std::vector<will_engine::DescriptorUniformData>& uniformBuffers, int index = -1) const;

    void destroyDescriptorBuffer(DescriptorBuffer& descriptorBuffer) const;

    VkShaderModule createShaderModule(const std::filesystem::path& path) const;

    void destroyShaderModule(VkShaderModule& shaderModule) const;


    VkPipelineLayout createPipelineLayout(const VkPipelineLayoutCreateInfo& createInfo) const;

    void destroyPipelineLayout(VkPipelineLayout& pipelineLayout) const;

    VkPipeline createRenderPipeline(PipelineBuilder& builder, const std::vector<VkDynamicState>& additionalDynamicStates = {}) const;

    VkPipeline createComputePipeline(const VkComputePipelineCreateInfo& pipelineInfo) const;

    void destroyPipeline(VkPipeline& pipeline) const;

    VkDescriptorSetLayout createDescriptorSetLayout(DescriptorLayoutBuilder& layoutBuilder, VkShaderStageFlagBits shaderStageFlags, VkDescriptorSetLayoutCreateFlagBits layoutCreateFlags) const;

    void destroyDescriptorSetLayout(VkDescriptorSetLayout& descriptorSetLayout) const;

    VkImageView createImageView(const VkImageViewCreateInfo& viewInfo) const;

    void destroyImageView(VkImageView& imageView) const;

public:
    [[nodiscard]] VkSampler getDefaultSamplerLinear() const { return defaultSamplerLinear; }
    [[nodiscard]] VkSampler getDefaultSamplerNearest() const { return defaultSamplerNearest; }
    [[nodiscard]] VkSampler getDefaultSamplerMipMappedNearest() const { return defaultSamplerMipMappedLinear; }
    [[nodiscard]] AllocatedImage getWhiteImage() const { return whiteImage; }
    [[nodiscard]] AllocatedImage getErrorCheckerboardImage() const { return errorCheckerboardImage; }

    [[nodiscard]] VkDescriptorSetLayout getEmptyLayout() const { return emptyDescriptorSetLayout; }
    [[nodiscard]] VkDescriptorSetLayout getSceneDataLayout() const { return sceneDataLayout; }
    [[nodiscard]] VkDescriptorSetLayout getFrustumCullLayout() const { return frustumCullLayout; }
    [[nodiscard]] VkDescriptorSetLayout getAddressesLayout() const { return addressesLayout; }
    [[nodiscard]] VkDescriptorSetLayout getTexturesLayout() const { return texturesLayout; }
    [[nodiscard]] VkDescriptorSetLayout getRenderTargetsLayout() const { return renderTargetsLayout; }
    [[nodiscard]] VkDescriptorSetLayout getTerrainTexturesLayout() const { return terrainTexturesLayout; }
    [[nodiscard]] VkDescriptorSetLayout getTerrainUniformLayout() const { return terrainUniformLayout; }

private:
    const VulkanContext& context;
    const ImmediateSubmitter& immediate;

    AllocatedImage whiteImage{};
    AllocatedImage errorCheckerboardImage{};
    VkSampler defaultSamplerLinear{VK_NULL_HANDLE};
    VkSampler defaultSamplerNearest{VK_NULL_HANDLE};
    VkSampler defaultSamplerMipMappedLinear{VK_NULL_HANDLE};

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

    VkDescriptorSetLayout terrainTexturesLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout terrainUniformLayout{VK_NULL_HANDLE};
};

class CustomIncluder final : public shaderc::CompileOptions::IncluderInterface
{
public:
    explicit CustomIncluder(const std::vector<std::string>& includePaths) : includePaths(includePaths) {}

    shaderc_include_result* GetInclude(const char* requestedSource, shaderc_include_type type, const char* requestingSource, size_t includeDepth) override
    {
        std::string resolvedPath;
        for (const auto& path : includePaths) {
            std::string fullPath = path + "/" + requestedSource;
            if (FileExists(fullPath)) {
                resolvedPath = fullPath;
                break;
            }
        }

        if (resolvedPath.empty()) {
            return CreateIncludeError("Failed to resolve include: " + std::string(requestedSource));
        }

        std::ifstream file(resolvedPath);
        if (!file.is_open()) {
            return CreateIncludeError("Failed to open include file: " + resolvedPath);
        }

        const std::string content((std::istreambuf_iterator(file)), std::istreambuf_iterator<char>());

        auto* result = new shaderc_include_result();
        result->source_name = strdup(resolvedPath.c_str());
        result->source_name_length = resolvedPath.size();
        result->content = strdup(content.c_str());
        result->content_length = content.size();
        result->user_data = nullptr;
        return result;
    }

    void ReleaseInclude(shaderc_include_result* data) override
    {
        free(const_cast<char*>(data->source_name));
        free(const_cast<char*>(data->content));
        delete data;
    }

private:
    std::vector<std::string> includePaths;

    bool FileExists(const std::string& path)
    {
        std::ifstream file(path);
        return file.good();
    }

    shaderc_include_result* CreateIncludeError(const std::string& message)
    {
        auto* result = new shaderc_include_result();
        result->source_name = strdup(message.c_str());
        result->source_name_length = message.size();
        result->content = nullptr;
        result->content_length = 0;
        result->user_data = nullptr;
        return result;
    }
};
}


#endif //RESOURCE_MANAGER_H
