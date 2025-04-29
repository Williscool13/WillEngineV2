//
// Created by William on 2024-12-13.
//

#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H
#include <array>
#include <filesystem>
#include <fstream>
#include <span>

#include "renderer_constants.h"
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

struct DestructorBufferData
{
    VkBuffer buffer{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
};

struct DestructorImageData
{
    VkImage image{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
};

struct DestructionQueue
{
    std::vector<DestructorBufferData> bufferQueue;
    std::vector<DestructorImageData> imageQueue;
    std::vector<VkImageView> imageViewQueue;
    std::vector<VkSampler> samplerQueue;
    std::vector<VkPipeline> pipelineQueue;
    std::vector<VkPipelineLayout> pipelineLayoutQueue;
    std::vector<VkDescriptorSetLayout> descriptorSetLayoutQueue;
};


struct BufferCopyInfo
{
    AllocatedBuffer src;
    VkDeviceSize srcOffset;
    AllocatedBuffer dst;
    VkDeviceSize dstOffset;
    VkDeviceSize size;
};

class ResourceManager
{
public:
    ResourceManager() = delete;

    ResourceManager(const VulkanContext& context, ImmediateSubmitter& immediate);

    ~ResourceManager();

public:
    /**
     * Meant to be called every frame, should be called before
     * @param currentFrameOverlap The current frame overlap used to know which deletion queue to flush.
     */
    void update(int32_t currentFrameOverlap);

    /**
     * Meant to be called when the program is finished on the cleanup step.
     */
    void flushDestructionQueue();

private:
    std::array<DestructionQueue, FRAME_OVERLAP> destructionQueues;

    int32_t lastKnownFrameOverlap{0};

public: // VkBuffer
    [[nodiscard]] AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) const;

    AllocatedBuffer createHostSequentialBuffer(size_t allocSize, VkBufferUsageFlags additionalUsages = 0) const;

    AllocatedBuffer createHostRandomBuffer(size_t allocSize, VkBufferUsageFlags additionalUsages = 0) const;

    AllocatedBuffer createDeviceBuffer(size_t allocSize, VkBufferUsageFlags additionalUsages = 0) const;

    [[nodiscard]] AllocatedBuffer createStagingBuffer(size_t allocSize) const;

    [[nodiscard]] AllocatedBuffer createReceivingBuffer(size_t allocSize) const;

    /**
     * Immediately destroys the buffer, typically used to destroy staging/upload buffers if the buffers in question are immediately used and synchronized using `ImmediateSubmitter`
     * \n should not be used otherwise, as the buffer may be destroyed before it is used.
     * @param buffer
     */
    void destroyBufferImmediate(AllocatedBuffer& buffer) const;

    void destroyBuffer(AllocatedBuffer& buffer);

public: // VkBuffer Helpers
    void copyBufferImmediate(const AllocatedBuffer& src, const AllocatedBuffer& dst, VkDeviceSize size) const;

    void copyBufferImmediate(const AllocatedBuffer& src, const AllocatedBuffer& dst, VkDeviceSize size, VkDeviceSize offset) const;

    /**
     * Copy buffer immediate should generally only be used during asset initialization.
     * \n For updates within the frame loop, use \code vk_helpers::copyBuffer\endcode
     * @param bufferCopyInfos
     */
    void copyBufferImmediate(std::span<BufferCopyInfo> bufferCopyInfos) const;

    [[nodiscard]] VkDeviceAddress getBufferAddress(const AllocatedBuffer& buffer) const;

public: // Samplers
    [[nodiscard]] VkSampler createSampler(const VkSamplerCreateInfo& createInfo) const;

    void destroySampler(VkSampler sampler);

public: // VkImage and VkImageView
    [[nodiscard]] AllocatedImage createImage(const VkImageCreateInfo& createInfo) const;

    [[nodiscard]] AllocatedImage createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false) const;

    [[nodiscard]] AllocatedImage createImage(const void* data, size_t dataSize, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                                             bool mipmapped = false) const;

    [[nodiscard]] AllocatedImage createCubemap(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false) const;

    void destroyImage(AllocatedImage& image);

public: // Descriptor Buffer
    [[nodiscard]] DescriptorBufferSampler createDescriptorBufferSampler(VkDescriptorSetLayout layout, int32_t maxObjectCount) const;

    int32_t setupDescriptorBufferSampler(DescriptorBufferSampler& descriptorBuffer, const std::vector<will_engine::DescriptorImageData>& imageBuffers,
                                         int index = -1) const;

    [[nodiscard]] DescriptorBufferUniform createDescriptorBufferUniform(VkDescriptorSetLayout layout, int32_t maxObjectCount) const;

    int32_t setupDescriptorBufferUniform(DescriptorBufferUniform& descriptorBuffer,
                                         const std::vector<will_engine::DescriptorUniformData>& uniformBuffers, int index = -1) const;

    void destroyDescriptorBuffer(DescriptorBuffer& descriptorBuffer);

public: // Shader Module
    VkShaderModule createShaderModule(const std::filesystem::path& path) const;

    /**
     * As far as I know shader modules don't need to defer destroyed, since it's used to construct the pipeline (without `ImmediateSubmitter`)
     * @param shaderModule
     */
    void destroyShaderModule(VkShaderModule& shaderModule) const;

public: // Pipeline Layout
    VkPipelineLayout createPipelineLayout(const VkPipelineLayoutCreateInfo& createInfo) const;

    void destroyPipelineLayout(VkPipelineLayout pipelineLayout);

public: // Pipelines
    VkPipeline createRenderPipeline(PipelineBuilder& builder, const std::vector<VkDynamicState>& additionalDynamicStates = {}) const;

    VkPipeline createComputePipeline(const VkComputePipelineCreateInfo& pipelineInfo) const;

    void destroyPipeline(VkPipeline pipeline);

public: // Descriptor Set Layout
    VkDescriptorSetLayout createDescriptorSetLayout(DescriptorLayoutBuilder& layoutBuilder, VkShaderStageFlagBits shaderStageFlags,
                                                    VkDescriptorSetLayoutCreateFlagBits layoutCreateFlags) const;

    void destroyDescriptorSetLayout(VkDescriptorSetLayout descriptorSetLayout);

public: // Image View
    VkImageView createImageView(const VkImageViewCreateInfo& viewInfo) const;

    void destroyImageView(VkImageView imageView);

public:
    [[nodiscard]] VkSampler getDefaultSamplerLinear() const { return defaultSamplerLinear; }
    [[nodiscard]] VkSampler getDefaultSamplerNearest() const { return defaultSamplerNearest; }
    [[nodiscard]] VkSampler getDefaultSamplerMipMappedNearest() const { return defaultSamplerMipMappedLinear; }
    [[nodiscard]] AllocatedImage getWhiteImage() const { return whiteImage; }
    [[nodiscard]] AllocatedImage getErrorCheckerboardImage() const { return errorCheckerboardImage; }

    [[nodiscard]] VkDescriptorSetLayout getEmptyLayout() const { return emptyDescriptorSetLayout; }
    [[nodiscard]] VkDescriptorSetLayout getSceneDataLayout() const { return sceneDataLayout; }
    [[nodiscard]] VkDescriptorSetLayout getFrustumCullLayout() const { return frustumCullLayout; }
    [[nodiscard]] VkDescriptorSetLayout getRenderObjectAddressesLayout() const { return addressesLayout; }
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

    shaderc_include_result* GetInclude(const char* requestedSource, shaderc_include_type type, const char* requestingSource,
                                       size_t includeDepth) override
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
