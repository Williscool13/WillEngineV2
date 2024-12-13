//
// Created by William on 2024-12-13.
//

#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H
#include "vk_types.h"


class ImmediateSubmitter;
class VulkanContext;

class ResourceManager
{
public:
    ResourceManager() = delete;

    ResourceManager(const VulkanContext& context, ImmediateSubmitter& immediate);

    ~ResourceManager();

    [[nodiscard]] AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) const;

    [[nodiscard]] AllocatedBuffer createStagingBuffer(size_t allocSize) const;

    [[nodiscard]] AllocatedBuffer createReceivingBuffer(size_t allocSize) const;

    void copyBuffer(const AllocatedBuffer& src, const AllocatedBuffer& dst, VkDeviceSize size) const;

    [[nodiscard]] VkDeviceAddress getBufferAddress(const AllocatedBuffer& buffer) const;

    void destroyBuffer(AllocatedBuffer& buffer) const;

    [[nodiscard]] AllocatedImage createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false) const;

    [[nodiscard]] AllocatedImage createImage(const void* data, size_t dataSize, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false) const;

    [[nodiscard]] AllocatedImage createCubemap(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false) const;

    void destroyImage(const AllocatedImage& img) const;

public:
    [[nodiscard]] VkSampler getDefaultSamplerLinear() const { return defaultSamplerLinear; }
    [[nodiscard]] VkSampler getDefaultSamplerNearest() const { return defaultSamplerNearest; }
    [[nodiscard]] AllocatedImage getWhiteImage() const { return whiteImage; }
    [[nodiscard]] AllocatedImage getErrorCheckerboardImage() const { return errorCheckerboardImage; }

private:
    const VulkanContext& context;
    const ImmediateSubmitter& immediate;

    AllocatedImage whiteImage{};
    AllocatedImage errorCheckerboardImage{};
    VkSampler defaultSamplerLinear{VK_NULL_HANDLE};
    VkSampler defaultSamplerNearest{VK_NULL_HANDLE};
};

#endif //RESOURCE_MANAGER_H
