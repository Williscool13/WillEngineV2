//
// Created by William on 8/11/2024.
//

#ifndef VKHELPERS_H
#define VKHELPERS_H
#include <vulkan/vulkan_core.h>
#include <vulkan/vk_enum_string_helper.h>
#include <glm/glm.hpp>
#include <fmt/format.h>

#include "vk_types.h"
#include "fastgltf/types.hpp"


class ImmediateSubmitter;
class ResourceManager;
class Engine;

namespace vk_helpers
{
VkImageCreateInfo imageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);

VkImageCreateInfo cubemapCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);

VkImageViewCreateInfo imageviewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);

VkImageViewCreateInfo cubemapViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);

VkImageSubresourceRange imageSubresourceRange(VkImageAspectFlags aspectMask);


VkCommandPoolCreateInfo commandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);

VkCommandBufferAllocateInfo commandBufferAllocateInfo(VkCommandPool pool, uint32_t count = 1);

VkCommandBufferBeginInfo commandBufferBeginInfo(VkCommandBufferUsageFlags flags = 0);

VkCommandBufferSubmitInfo commandBufferSubmitInfo(VkCommandBuffer cmd);


VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags = 0);

VkSemaphoreCreateInfo semaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0);

VkSemaphoreSubmitInfo semaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);

VkRenderingAttachmentInfo attachmentInfo(VkImageView view, const VkClearValue* clear,
                                         VkImageLayout layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/);

VkRenderingInfo renderingInfo(VkExtent2D renderExtent, const VkRenderingAttachmentInfo* colorAttachment, const VkRenderingAttachmentInfo* depthAttachment);

VkSubmitInfo2 submitInfo(const VkCommandBufferSubmitInfo* cmd, const VkSemaphoreSubmitInfo* signalSemaphoreInfo, const VkSemaphoreSubmitInfo* waitSemaphoreInfo);

VkPresentInfoKHR presentInfo();


/**
 * Returns the Buffer Device address of the specified buffer
 * @param device The device the buffer was created with
 * @param buffer The buffer whose address will be returned
 * @return the address of the buffer
 */
VkDeviceAddress getDeviceAddress(VkDevice device, VkBuffer buffer);

/**
 * Returns the final aligned size based on the alignment given
 * @param value value to align
 * @param alignment value to align to
 * @return
 */
VkDeviceSize getAlignedSize(VkDeviceSize value, VkDeviceSize alignment);


void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout targetLayout, VkImageAspectFlags aspectMask);

void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageAspectFlags aspectMask, VkImageLayout targetLayout);

void copyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);

void copyDepthToDepth(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);

void generateMipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);


VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo();

VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule shaderModule,
                                                              const char* entry = "main");

/**
 * Loads a shader module from the file paths specified
 * @param filePath
 * @param device
 * @param outShaderModule
 * @return
 */
bool loadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);


VkFilter extractFilter(fastgltf::Filter filter);

VkSamplerMipmapMode extractMipmapMode(fastgltf::Filter filter);

std::optional<AllocatedImage> loadImage(const ResourceManager& resourceManager, const fastgltf::Asset& asset, const fastgltf::Image& image, const std::filesystem::path& parentFolder);

/**
 * Loads a fastgltf texture.
 * @param texture
 * @param gltf
 * @param imageIndex
 * @param samplerIndex
 * @param imageOffset
 * @param samplerOffset
 */
void loadTexture(const fastgltf::Optional<fastgltf::TextureInfo>& texture, const fastgltf::Asset& gltf, int& imageIndex, int& samplerIndex,
                 uint32_t
                 imageOffset = 0, uint32_t samplerOffset = 0);

void saveImageRGBA32F(const ResourceManager& resourceManager, const ImmediateSubmitter& immediate, const AllocatedImage& image, VkImageLayout imageLayout, VkImageAspectFlags aspectFlag,
                      const char* savePath, bool overrideAlpha = true);

void saveImageRGBA16SFLOAT(const ResourceManager& resourceManager, const ImmediateSubmitter& immediate, const AllocatedImage& image, VkImageLayout imageLayout, VkImageAspectFlags aspectFlag,
                           const char* savePath, bool overrideAlpha = true);

void savePacked32Bit(const ResourceManager& resourceManager, const ImmediateSubmitter& immediate, const AllocatedImage& image, VkImageLayout imageLayout, VkImageAspectFlags aspectFlag,
                     const char* savePath, const std::function<glm::vec4(uint32_t)>& unpackingFunction);

void savePacked64Bit(const ResourceManager& resourceManager, const ImmediateSubmitter& immediate, const AllocatedImage& image, VkImageLayout imageLayout, VkImageAspectFlags aspectFlag,
                     const char* savePath, const std::function<glm::vec4(uint64_t)>& unpackingFunction);

/**
 * Save the Allocated image as a grayscaled image. The image must be a format with only 1 channel (e.g. R32 or D32)
 */
void saveImageR32F(const ResourceManager& resourceManager, const ImmediateSubmitter& immediate, const AllocatedImage& image, VkImageLayout imageLayout, VkImageAspectFlags aspectFlag,
                   const char* savePath, const std::function<float(float)>& valueTransform);
}


#define VK_CHECK(x)                                                          \
    do {                                                                     \
        VkResult err = x;                                                    \
        if (err) {                                                           \
            fmt::print("Detected Vulkan error: {}\n", string_VkResult(err)); \
            abort();                                                         \
        }                                                                    \
    } while (0)

#endif //VKHELPERS_H
