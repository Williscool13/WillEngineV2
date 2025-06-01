//
// Created by William on 2024-12-13.
//

#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H
#include <array>
#include <filesystem>
#include <fstream>
#include <span>
#include <variant>
#include <vma/vk_mem_alloc.h>

#include <volk/volk.h>


#include "renderer_constants.h"
#include "vk_types.h"
#include "shaderc/shaderc.h"
#include "shaderc/shaderc.hpp"
#include "ktxvulkan.h"
#include "vulkan_context.h"
#include "resources/descriptor_set_layout.h"
#include "resources/image_resource.h"
#include "resources/resources_fwd.h"
#include "resources/sampler.h"


namespace will_engine
{
class VulkanContext;
}


namespace will_engine::renderer
{
class Image;
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
    std::vector<std::unique_ptr<VulkanResource> > resources{};

    void flush()
    {
        resources.clear();
    }
};


struct BufferCopyInfo
{
    VkBuffer src;
    VkDeviceSize srcOffset;
    VkBuffer dst;
    VkDeviceSize dstOffset;
    VkDeviceSize size;
};

struct ImageFormatProperties
{
    VkResult result;
    VkImageFormatProperties properties;
};

class ResourceManager
{
public:
    ResourceManager() = delete;

    ResourceManager(VulkanContext& context, ImmediateSubmitter& immediate);

    ~ResourceManager();

public:
    VkDevice getDevice() const { return context.device; }
    VmaAllocator getAllocator() const { return context.allocator; }

    const VkPhysicalDeviceDescriptorBufferPropertiesEXT& getPhysicalDeviceDescriptorBufferProperties() const
    {
        return context.deviceDescriptorBufferProperties;
    }

    [[nodiscard]] ktxVulkanDeviceInfo* getKtxVulkanDeviceInfo() const { return vulkanDeviceInfo; }

private:
    VulkanContext& context;
    const ImmediateSubmitter& immediate;

    VkCommandPool ktxTextureCommandPool{VK_NULL_HANDLE};
    ktxVulkanDeviceInfo* vulkanDeviceInfo{nullptr};

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

    ImageFormatProperties getPhysicalDeviceImageFormatProperties(VkFormat format, VkImageUsageFlags usageFlags) const;

private:
    std::array<DestructionQueue, FRAME_OVERLAP> destructionQueues;

    int32_t lastKnownFrameOverlap{0};

public:
    void destroyResource(std::unique_ptr<VulkanResource> resource)
    {
        if (!resource) { return; }
        destructionQueues[lastKnownFrameOverlap].resources.emplace_back(std::move(resource));
    }

    void destroyResourceImmediate(std::unique_ptr<VulkanResource> resource)
    {
        if (!resource) { return; }
        resource.reset(); // explicit cleanup
    }

public: // VkBuffer Helpers
    void copyBufferImmediate(VkBuffer src, VkBuffer dst, VkDeviceSize size) const;

    void copyBufferImmediate(VkBuffer src, VkBuffer dst, VkDeviceSize size, VkDeviceSize offset) const;

    /**
     * Copy buffer immediate should generally only be used during asset initialization.
     * \n For updates within the frame loop, use \code vk_helpers::copyBuffer\endcode
     * @param bufferCopyInfos
     */
    void copyBufferImmediate(std::span<BufferCopyInfo> bufferCopyInfos) const;

    [[nodiscard]] VkDeviceAddress getBufferAddress(const Buffer& buffer) const;

    [[nodiscard]] VkDeviceAddress getBufferAddress(VkBuffer buffer) const;

public:
    template<typename T, typename... Args>
    std::unique_ptr<T> createResource(Args&&... args)
    {
        return std::make_unique<T>(this, std::forward<Args>(args)...);
    }

    template<typename T, typename... Args>
    std::shared_ptr<T> createResourceShared(Args&&... args)
    {
        return std::make_shared<T>(this, std::forward<Args>(args)...);
    }

public: // Special helpers for unique resources
    ImageResourcePtr createCubemapImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);

    ImageResourcePtr createImageFromData(const void* data, size_t dataSize, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                                         bool mipmapped = false);

public:
    // All the resources below are guaranteed to live for the duration of the application
    [[nodiscard]] VkSampler getDefaultSamplerLinear() const { return defaultSamplerLinear->sampler; }
    [[nodiscard]] VkSampler getDefaultSamplerNearest() const { return defaultSamplerNearest->sampler; }
    [[nodiscard]] VkSampler getDefaultSamplerMipMappedNearest() const { return defaultSamplerMipMappedLinear->sampler; }
    [[nodiscard]] VkImageView getWhiteImage() const { return whiteImage->imageView; }
    [[nodiscard]] VkImageView getErrorCheckerboardImage() const { return errorCheckerboardImage->imageView; }

    [[nodiscard]] VkDescriptorSetLayout getEmptyLayout() const { return emptyDescriptorSetLayout->layout; }
    [[nodiscard]] VkDescriptorSetLayout getSceneDataLayout() const { return sceneDataLayout->layout; }
    [[nodiscard]] VkDescriptorSetLayout getFrustumCullLayout() const { return frustumCullLayout->layout; }
    [[nodiscard]] VkDescriptorSetLayout getRenderObjectAddressesLayout() const { return addressesLayout->layout; }
    [[nodiscard]] VkDescriptorSetLayout getTexturesLayout() const { return texturesLayout->layout; }
    [[nodiscard]] VkDescriptorSetLayout getRenderTargetsLayout() const { return renderTargetsLayout->layout; }
    [[nodiscard]] VkDescriptorSetLayout getTerrainTexturesLayout() const { return terrainTexturesLayout->layout; }
    [[nodiscard]] VkDescriptorSetLayout getTerrainUniformLayout() const { return terrainUniformLayout->layout; }

private:
    ImageResourcePtr whiteImage{};
    ImageResourcePtr errorCheckerboardImage{};
    SamplerPtr defaultSamplerLinear{};
    SamplerPtr defaultSamplerNearest{};
    SamplerPtr defaultSamplerMipMappedLinear{};

    DescriptorSetLayoutPtr emptyDescriptorSetLayout{};

    DescriptorSetLayoutPtr sceneDataLayout{};
    DescriptorSetLayoutPtr frustumCullLayout{};
    /**
     * Material and Instance Buffer Addresses
     */
    DescriptorSetLayoutPtr addressesLayout{};
    /**
     * Sampler and Image Arrays
     */
    DescriptorSetLayoutPtr texturesLayout{};

    /**
     * Used in deferred resolve
     */
    DescriptorSetLayoutPtr renderTargetsLayout{};

    DescriptorSetLayoutPtr terrainTexturesLayout{};
    DescriptorSetLayoutPtr terrainUniformLayout{};
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
