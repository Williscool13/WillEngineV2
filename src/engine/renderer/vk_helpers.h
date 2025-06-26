//
// Created by William on 8/11/2024.
//

#ifndef VKHELPERS_H
#define VKHELPERS_H
#include <filesystem>
#include <functional>
#include <span>

#include <vulkan/vulkan_core.h>
#include <vulkan/vk_enum_string_helper.h>

#include <fmt/format.h>
#include <glm/glm.hpp>
#include <half/half.hpp>

#include "resource_manager.h"
#include "engine/renderer/resources/resources_fwd.h"

struct DescriptorLayoutBuilder;

namespace will_engine::renderer
{
namespace vk_helpers
{
    // Enum to define supported image formats
    enum class ImageFormat
    {
        RGBA32F,
        RGBA16F,
        RGBA8,
        RGBA8_UNORM,
        A2R10G10B10_UNORM,
        R32F,
        R16F,
        R8UNORM,
        R8UINT,
        D32S8
    };

    // Structure to hold format-specific parameters
    struct FormatInfo
    {
        uint64_t bytesPerPixel;
        VkImageAspectFlags aspectMask;
    };

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

    VkRenderingInfo renderingInfo(VkExtent2D renderExtent, const VkRenderingAttachmentInfo* colorAttachment,
                                  const VkRenderingAttachmentInfo* depthAttachment);

    VkSubmitInfo2 submitInfo(const VkCommandBufferSubmitInfo* cmd, const VkSemaphoreSubmitInfo* signalSemaphoreInfo,
                             const VkSemaphoreSubmitInfo* waitSemaphoreInfo);

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

    void copyBuffer(VkCommandBuffer cmd, VkBuffer src, VkDeviceSize srcOffset, VkBuffer dst, VkDeviceSize dstOffset, VkDeviceSize size);

    void clearColorImage(VkCommandBuffer cmd, VkImageAspectFlags aspectFlag, ImageResource* image, VkImageLayout dstLayout,
                         VkClearColorValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f});

    void clearColorImage(VkCommandBuffer cmd, VkImageAspectFlags aspectFlag, VkImage image, VkImageLayout srcLayout, VkImageLayout dstLayout,
                         VkClearColorValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f});

    void imageBarrier(VkCommandBuffer cmd, ImageResource* image, VkImageLayout targetLayout, VkImageAspectFlags aspectMask);

    void imageBarrier(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout targetLayout, VkImageAspectFlags aspectMask);

    void bufferBarrier(VkCommandBuffer cmd, VkBuffer buffer, VkPipelineStageFlagBits2 srcPipelineStage,
                        VkAccessFlagBits2 srcAccessBit, VkPipelineStageFlagBits2
                        dstPipelineStage, VkAccessFlagBits2 dstAccessBit);

    void copyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);
    void copyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent3D srcSize, VkExtent3D dstSize);

    void generateMipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);

    void generateMipmapsCubemap(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize, VkImageLayout inputLayout, VkImageLayout ouputLayout);


    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo();

    VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule shaderModule,
                                                                  const char* entry = "main");

    /**
     * Save the Allocated image as a grayscaled image. The image must be a format with only 1 channel (e.g. R32 or D32)
     */
    void saveImageR32F(ResourceManager& resourceManager, const ImmediateSubmitter& immediate,
                       const ImageResource* image,
                       VkImageLayout imageLayout, VkImageAspectFlags aspectFlag, const char* savePath,
                       const std::function<float(float)>& valueTransform, int32_t mipLevel = 0);

    void saveHeightmap(const std::vector<float>& heightData, int width, int height, const std::filesystem::path& filename);

    FormatInfo getFormatInfo(ImageFormat format, bool stencilOnly);

    void processImageData(void* sourceData, std::span<uint8_t> targetData,
                          uint32_t pixelCount,
                          ImageFormat format, bool stencilOnly = false);

    /**
     * Used to save any type of image
     * @param resourceManager
     * @param immediate
     * @param image
     * @param format
     * @param savePath
     * @param saveStencilOnly value only used if aspect mask is depthStencil
     */
    void saveImage(ResourceManager& resourceManager,
                   const ImmediateSubmitter& immediate,
                   ImageResource* image,
                   ImageFormat format,
                   const std::string& savePath,
                   bool saveStencilOnly = false);
}
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
