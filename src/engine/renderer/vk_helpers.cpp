//
// Created by William on 8/11/2024.
//

#include "vk_helpers.h"

#include <filesystem>
#include <fstream>
#include <glm/gtc/packing.hpp>
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>
#include "volk/volk.h"
#include "half/half.hpp"

#include "immediate_submitter.h"
#include "resource_manager.h"

VkImageCreateInfo will_engine::vk_helpers::imageCreateInfo(const VkFormat format, const VkImageUsageFlags usageFlags, const VkExtent3D extent)
{
    VkImageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.pNext = nullptr;

    // Single 2D image with no mip levels by default
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = format;
    info.extent = extent;
    info.mipLevels = 1;
    info.arrayLayers = 1;

    // No MSAA
    info.samples = VK_SAMPLE_COUNT_1_BIT;

    // Optimal has best performance
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = usageFlags;

    return info;
}

VkImageCreateInfo will_engine::vk_helpers::cubemapCreateInfo(const VkFormat format, const VkImageUsageFlags usageFlags, const VkExtent3D extent)
{
    VkImageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.pNext = nullptr;

    info.imageType = VK_IMAGE_TYPE_2D;

    info.format = format;
    info.extent = extent;

    info.mipLevels = 1;
    info.arrayLayers = 6;

    //for MSAA. we will not be using it by default, so default it to 1 sample per pixel.
    info.samples = VK_SAMPLE_COUNT_1_BIT;

    //optimal tiling, which means the image is stored on the best gpu format
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = usageFlags;

    info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    return info;
}

VkImageViewCreateInfo will_engine::vk_helpers::imageviewCreateInfo(const VkFormat format, const VkImage image, const VkImageAspectFlags aspectFlags)
{
    // Identical to imageCreateInfo, but is imageView instead
    VkImageViewCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.pNext = nullptr;

    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.image = image;
    info.format = format;

    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;
    info.subresourceRange.aspectMask = aspectFlags;

    return info;
}

VkImageViewCreateInfo will_engine::vk_helpers::cubemapViewCreateInfo(const VkFormat format, const VkImage image, const VkImageAspectFlags aspectFlags)
{
    // build a image-view for the depth image to use for rendering
    VkImageViewCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.pNext = nullptr;

    info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    info.image = image;
    info.format = format;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 6;
    info.subresourceRange.aspectMask = aspectFlags;

    return info;
}

VkImageSubresourceRange will_engine::vk_helpers::imageSubresourceRange(const VkImageAspectFlags aspectMask)
{
    VkImageSubresourceRange subImage{};
    subImage.aspectMask = aspectMask;
    subImage.baseMipLevel = 0;
    subImage.levelCount = VK_REMAINING_MIP_LEVELS;
    //subImage.levelCount = 1;
    subImage.baseArrayLayer = 0;
    subImage.layerCount = VK_REMAINING_ARRAY_LAYERS;
    //subImage.layerCount = 1;

    return subImage;
}


VkCommandPoolCreateInfo will_engine::vk_helpers::commandPoolCreateInfo(uint32_t queueFamilyIndex, const VkCommandPoolCreateFlags flags /*= 0*/)
{
    VkCommandPoolCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = flags;
    return info;
}

// ReSharper disable once CppParameterMayBeConst
VkCommandBufferAllocateInfo will_engine::vk_helpers::commandBufferAllocateInfo(VkCommandPool pool, const uint32_t count /*= 1*/)
{
    VkCommandBufferAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.pNext = nullptr;

    info.commandPool = pool;
    info.commandBufferCount = count;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    return info;
}

VkCommandBufferBeginInfo will_engine::vk_helpers::commandBufferBeginInfo(const VkCommandBufferUsageFlags flags)
{
    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.pNext = nullptr;

    info.pInheritanceInfo = nullptr;
    info.flags = flags;
    return info;
}

// ReSharper disable once CppParameterMayBeConst
VkCommandBufferSubmitInfo will_engine::vk_helpers::commandBufferSubmitInfo(VkCommandBuffer cmd)
{
    VkCommandBufferSubmitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    info.pNext = nullptr;
    info.commandBuffer = cmd;
    info.deviceMask = 0;

    return info;
}


VkFenceCreateInfo will_engine::vk_helpers::fenceCreateInfo(const VkFenceCreateFlags flags /*= 0*/)
{
    VkFenceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = flags;
    return info;
}

VkSemaphoreCreateInfo will_engine::vk_helpers::semaphoreCreateInfo(const VkSemaphoreCreateFlags flags /*= 0*/)
{
    VkSemaphoreCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = flags;
    return info;
}

// ReSharper disable once CppParameterMayBeConst
VkSemaphoreSubmitInfo will_engine::vk_helpers::semaphoreSubmitInfo(const VkPipelineStageFlags2 stageMask, VkSemaphore semaphore)
{
    VkSemaphoreSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.semaphore = semaphore;
    submitInfo.stageMask = stageMask;
    submitInfo.deviceIndex = 0;
    submitInfo.value = 1;

    return submitInfo;
}

// ReSharper disable once CppParameterMayBeConst
VkRenderingAttachmentInfo will_engine::vk_helpers::attachmentInfo(VkImageView view, const VkClearValue* clear, const VkImageLayout layout)
{
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.pNext = nullptr;

    colorAttachment.imageView = view;
    colorAttachment.imageLayout = layout;
    colorAttachment.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    if (clear) {
        colorAttachment.clearValue = *clear;
    }

    return colorAttachment;
}

VkRenderingInfo will_engine::vk_helpers::renderingInfo(const VkExtent2D renderExtent, const VkRenderingAttachmentInfo* colorAttachment,
                                                       const VkRenderingAttachmentInfo* depthAttachment)
{
    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.pNext = nullptr;

    renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, renderExtent};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = colorAttachment == nullptr ? 0 : 1;
    renderInfo.pColorAttachments = colorAttachment;
    renderInfo.pDepthAttachment = depthAttachment;
    renderInfo.pStencilAttachment = nullptr;

    return renderInfo;
}

VkSubmitInfo2 will_engine::vk_helpers::submitInfo(const VkCommandBufferSubmitInfo* cmd, const VkSemaphoreSubmitInfo* signalSemaphoreInfo,
                                                  const VkSemaphoreSubmitInfo* waitSemaphoreInfo)
{
    VkSubmitInfo2 info = {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    info.pNext = nullptr;

    info.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0 : 1;
    info.pWaitSemaphoreInfos = waitSemaphoreInfo;

    info.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0 : 1;
    info.pSignalSemaphoreInfos = signalSemaphoreInfo;

    info.commandBufferInfoCount = 1;
    info.pCommandBufferInfos = cmd;

    return info;
}

// ReSharper disable twice CppParameterMayBeConst
VkDeviceAddress will_engine::vk_helpers::getDeviceAddress(VkDevice device, VkBuffer buffer)
{
    VkBufferDeviceAddressInfo deviceAdressInfo{};
    deviceAdressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    deviceAdressInfo.buffer = buffer;
    const uint64_t address = vkGetBufferDeviceAddress(device, &deviceAdressInfo);

    return address;
}


VkDeviceSize will_engine::vk_helpers::getAlignedSize(const VkDeviceSize value, VkDeviceSize alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

void will_engine::vk_helpers::copyBuffer(VkCommandBuffer cmd, const renderer::AllocatedBuffer& src, VkDeviceSize srcOffset, const renderer::AllocatedBuffer& dst,
                                         VkDeviceSize dstOffset, VkDeviceSize size)
{
    if (src.buffer == VK_NULL_HANDLE) { return; }
    if (dst.buffer == VK_NULL_HANDLE) { return; }
    VkBufferCopy vertexCopy{};
    vertexCopy.dstOffset = dstOffset;
    vertexCopy.srcOffset = srcOffset;
    vertexCopy.size = size;

    vkCmdCopyBuffer(cmd, src.buffer, dst.buffer, 1, &vertexCopy);
}

void will_engine::vk_helpers::clearColorImage(VkCommandBuffer cmd, VkImageAspectFlags aspectFlag, VkImage image, VkImageLayout srcLayout,
                                              VkImageLayout dstLayout, VkClearColorValue clearColor)
{
    imageBarrier(cmd, image, srcLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, aspectFlag);
    VkImageSubresourceRange range{
        .aspectMask = aspectFlag,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1
    };
    vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
    imageBarrier(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, dstLayout, aspectFlag);
}


void will_engine::vk_helpers::imageBarrier(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout targetLayout,
                                              VkImageAspectFlags aspectMask)
{
    VkImageMemoryBarrier2 imageBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    imageBarrier.pNext = nullptr;

    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT;

    imageBarrier.oldLayout = currentLayout;
    imageBarrier.newLayout = targetLayout;

    imageBarrier.subresourceRange = imageSubresourceRange(aspectMask);
    imageBarrier.image = image;

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;

    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);
}

void will_engine::vk_helpers::uniformBarrier(VkCommandBuffer cmd, const renderer::AllocatedBuffer& buffer, VkPipelineStageFlagBits2 srcPipelineStage,
                                                 VkAccessFlagBits2 srcAccessBit, VkPipelineStageFlagBits2 dstPipelineStage,
                                                 VkAccessFlagBits2 dstAccessBit)
{
    VkBufferMemoryBarrier2 bufferBarrier{};
    bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bufferBarrier.pNext = nullptr;

    bufferBarrier.srcStageMask = srcPipelineStage;
    bufferBarrier.srcAccessMask = srcAccessBit;

    bufferBarrier.dstStageMask = dstPipelineStage;
    bufferBarrier.dstAccessMask = dstAccessBit;

    bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.buffer = buffer.buffer;
    bufferBarrier.offset = 0;
    bufferBarrier.size = VK_WHOLE_SIZE;

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;
    depInfo.dependencyFlags = 0;
    depInfo.bufferMemoryBarrierCount = 1;
    depInfo.pBufferMemoryBarriers = &bufferBarrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);
}

void will_engine::vk_helpers::copyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize)
{
    VkImageBlit2 blitRegion{.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr};

    blitRegion.srcOffsets[1].x = srcSize.width;
    blitRegion.srcOffsets[1].y = srcSize.height;
    blitRegion.srcOffsets[1].z = 1;

    blitRegion.dstOffsets[1].x = dstSize.width;
    blitRegion.dstOffsets[1].y = dstSize.height;
    blitRegion.dstOffsets[1].z = 1;

    blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcSubresource.layerCount = 1;
    blitRegion.srcSubresource.mipLevel = 0;

    blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.dstSubresource.baseArrayLayer = 0;
    blitRegion.dstSubresource.layerCount = 1;
    blitRegion.dstSubresource.mipLevel = 0;

    VkBlitImageInfo2 blitInfo{.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr};
    blitInfo.dstImage = destination;
    blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blitInfo.srcImage = source;
    blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blitInfo.filter = VK_FILTER_LINEAR;
    blitInfo.regionCount = 1;
    blitInfo.pRegions = &blitRegion;

    vkCmdBlitImage2(cmd, &blitInfo);
}

void will_engine::vk_helpers::generateMipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize)
{
    const int mipLevels = static_cast<int>(std::floor(std::log2(std::max(imageSize.width, imageSize.height)))) + 1;
    for (int mip = 0; mip < mipLevels; mip++) {
        VkExtent2D halfSize = imageSize;
        halfSize.width /= 2;
        halfSize.height /= 2;

        VkImageMemoryBarrier2 imageBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, .pNext = nullptr};

        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        constexpr VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBarrier.subresourceRange = imageSubresourceRange(aspectMask);
        imageBarrier.subresourceRange.levelCount = 1;
        imageBarrier.subresourceRange.baseMipLevel = mip;
        imageBarrier.image = image;

        VkDependencyInfo depInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .pNext = nullptr};
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &imageBarrier;

        vkCmdPipelineBarrier2(cmd, &depInfo);

        if (mip < mipLevels - 1) {
            VkImageBlit2 blitRegion{.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr};

            blitRegion.srcOffsets[1].x = imageSize.width;
            blitRegion.srcOffsets[1].y = imageSize.height;
            blitRegion.srcOffsets[1].z = 1;

            blitRegion.dstOffsets[1].x = halfSize.width;
            blitRegion.dstOffsets[1].y = halfSize.height;
            blitRegion.dstOffsets[1].z = 1;

            blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.srcSubresource.baseArrayLayer = 0;
            blitRegion.srcSubresource.layerCount = 1;
            blitRegion.srcSubresource.mipLevel = mip;

            blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.dstSubresource.baseArrayLayer = 0;
            blitRegion.dstSubresource.layerCount = 1;
            blitRegion.dstSubresource.mipLevel = mip + 1;

            VkBlitImageInfo2 blitInfo{.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr};
            blitInfo.dstImage = image;
            blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            blitInfo.srcImage = image;
            blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            blitInfo.filter = VK_FILTER_LINEAR;
            blitInfo.regionCount = 1;
            blitInfo.pRegions = &blitRegion;

            vkCmdBlitImage2(cmd, &blitInfo);

            imageSize = halfSize;
        }
    }

    // transition all mip levels into the final read_only layout
    imageBarrier(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
}

void will_engine::vk_helpers::generateMipmapsCubemap(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize, VkImageLayout inputLayout,
                                                     VkImageLayout ouputLayout)
{
    imageBarrier(cmd, image, inputLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

    const int mipLevels = static_cast<int>(std::floor(std::log2(std::max(imageSize.width, imageSize.height)))) + 1;
    for (int mip = 0; mip < mipLevels; mip++) {
        VkExtent2D halfSize = imageSize;
        halfSize.width /= 2;
        halfSize.height /= 2;

        VkImageMemoryBarrier2 imageBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, .pNext = nullptr};

        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        constexpr VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBarrier.subresourceRange = imageSubresourceRange(aspectMask);
        imageBarrier.subresourceRange.levelCount = 1;
        imageBarrier.subresourceRange.baseMipLevel = mip;
        imageBarrier.image = image;

        VkDependencyInfo depInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .pNext = nullptr};
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &imageBarrier;

        vkCmdPipelineBarrier2(cmd, &depInfo);

        if (mip < mipLevels - 1) {
            VkImageBlit2 blitRegion{.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr};

            blitRegion.srcOffsets[1].x = imageSize.width;
            blitRegion.srcOffsets[1].y = imageSize.height;
            blitRegion.srcOffsets[1].z = 1;

            blitRegion.dstOffsets[1].x = halfSize.width;
            blitRegion.dstOffsets[1].y = halfSize.height;
            blitRegion.dstOffsets[1].z = 1;

            blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.srcSubresource.baseArrayLayer = 0;
            blitRegion.srcSubresource.layerCount = 1;
            blitRegion.srcSubresource.mipLevel = mip;

            blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.dstSubresource.baseArrayLayer = 0;
            blitRegion.dstSubresource.layerCount = 1;
            blitRegion.dstSubresource.mipLevel = mip + 1;

            VkBlitImageInfo2 blitInfo{.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr};
            blitInfo.dstImage = image;
            blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            blitInfo.srcImage = image;
            blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            blitInfo.filter = VK_FILTER_LINEAR;
            blitInfo.regionCount = 1;
            blitInfo.pRegions = &blitRegion;

            vkCmdBlitImage2(cmd, &blitInfo);

            imageSize = halfSize;
        }
    }

    // transition all mip levels into the final read_only layout
    imageBarrier(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, ouputLayout, VK_IMAGE_ASPECT_COLOR_BIT);
}

VkPipelineLayoutCreateInfo will_engine::vk_helpers::pipelineLayoutCreateInfo()
{
    VkPipelineLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.pNext = nullptr;

    // empty defaults
    info.flags = 0;
    info.setLayoutCount = 0;
    info.pSetLayouts = nullptr;
    info.pushConstantRangeCount = 0;
    info.pPushConstantRanges = nullptr;
    return info;
}

VkPipelineShaderStageCreateInfo will_engine::vk_helpers::pipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule shaderModule,
                                                                                       const char* entry)
{
    VkPipelineShaderStageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.pNext = nullptr;

    // shader stage
    info.stage = stage;
    // module containing the code for this shader stage
    info.module = shaderModule;
    info.pName = entry; // usually "main"
    return info;
}

void will_engine::vk_helpers::saveImageR32F(renderer::ResourceManager& resourceManager, const renderer::ImmediateSubmitter& immediate, const renderer::AllocatedImage& image,
                                            VkImageLayout imageLayout, VkImageAspectFlags aspectFlag,
                                            const char* savePath, const std::function<float(float)>& valueTransform, int32_t mipLevel)
{
    size_t newXSize = image.imageExtent.width / static_cast<size_t>(std::pow(2, mipLevel));
    size_t newYSize = image.imageExtent.height / static_cast<size_t>(std::pow(2, mipLevel));
    const size_t texelCount = newXSize * newYSize;
    const size_t dataSize = texelCount * 1 * sizeof(float);
    renderer::AllocatedBuffer receivingBuffer = resourceManager.createReceivingBuffer(dataSize);

    immediate.submit([&, mipLevel](VkCommandBuffer cmd) {
        VkBufferImageCopy bufferCopyRegion{};
        bufferCopyRegion.imageSubresource.aspectMask = aspectFlag;
        bufferCopyRegion.imageSubresource.mipLevel = mipLevel;
        bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent = {static_cast<uint32_t>(newXSize), static_cast<uint32_t>(newYSize), 1u};
        bufferCopyRegion.bufferOffset = 0;
        bufferCopyRegion.bufferRowLength = 0;
        bufferCopyRegion.bufferImageHeight = 0;

        imageBarrier(cmd, image.image, imageLayout, VK_IMAGE_LAYOUT_GENERAL, aspectFlag);

        vkCmdCopyImageToBuffer(cmd, image.image, VK_IMAGE_LAYOUT_GENERAL, receivingBuffer.buffer, 1, &bufferCopyRegion);

        imageBarrier(cmd, image.image, VK_IMAGE_LAYOUT_GENERAL, imageLayout, aspectFlag);
    });

    void* data = receivingBuffer.info.pMappedData;
    const auto imageData = static_cast<float*>(data);

    const auto byteImageData = new uint8_t[texelCount * 4];
    const auto powEight = static_cast<float>(pow(2, 8) - 1);
    for (size_t i = 0; i < texelCount; ++i) {
        const float floatValue = valueTransform(imageData[i]);
        const auto value = static_cast<uint8_t>(floatValue * powEight);
        byteImageData[i * 4 + 0] = value;
        byteImageData[i * 4 + 1] = value;
        byteImageData[i * 4 + 2] = value;
        byteImageData[i * 4 + 3] = 255;
    }

    stbi_write_png(savePath, static_cast<int>(newXSize), static_cast<int>(newYSize), 4, byteImageData, static_cast<int>(newXSize) * 4);

    delete[] byteImageData;
    resourceManager.destroyResourceImmediate(receivingBuffer);
}

void will_engine::vk_helpers::saveHeightmap(const std::vector<float>& heightData, int width, int height, const std::filesystem::path& filename)
{
    float minHeight = std::numeric_limits<float>::max();
    float maxHeight = std::numeric_limits<float>::lowest();

    for (const float _height : heightData) {
        minHeight = std::min(minHeight, _height);
        maxHeight = std::max(maxHeight, _height);
    }

    const auto byteImageData = new uint8_t[width * height];

    for (size_t i = 0; i < width * height; ++i) {
        float normalizedHeight = (heightData[i] - minHeight) / (maxHeight - minHeight);
        normalizedHeight = std::max(0.0f, std::min(1.0f, normalizedHeight));
        byteImageData[i] = static_cast<uint8_t>(std::lround(normalizedHeight * 255.0f));
    }

    stbi_write_png(filename.string().c_str(), width, height, 1, byteImageData, width);

    delete[] byteImageData;
}

will_engine::vk_helpers::FormatInfo will_engine::vk_helpers::getFormatInfo(ImageFormat format, bool stencilOnly)
{
    switch (format) {
        case ImageFormat::RGBA32F:
            return {16, VK_IMAGE_ASPECT_COLOR_BIT};
        case ImageFormat::RGBA16F:
            return {sizeof(half_float::half) * 4, VK_IMAGE_ASPECT_COLOR_BIT};
        case ImageFormat::RGBA8:
            return {4, VK_IMAGE_ASPECT_COLOR_BIT};
        case ImageFormat::RGBA8_UNORM:
            return {sizeof(uint8_t) * 4, VK_IMAGE_ASPECT_COLOR_BIT};
        case ImageFormat::R32F:
            return {4, VK_IMAGE_ASPECT_COLOR_BIT};
        case ImageFormat::R16F:
            return {2, VK_IMAGE_ASPECT_COLOR_BIT};
        case ImageFormat::R8UNORM:
            return {1, VK_IMAGE_ASPECT_COLOR_BIT};
        case ImageFormat::R8UINT:
            return {1, VK_IMAGE_ASPECT_COLOR_BIT};
        case ImageFormat::A2R10G10B10_UNORM:
            return {sizeof(uint32_t), VK_IMAGE_ASPECT_COLOR_BIT};
        case ImageFormat::D32S8:
            return {stencilOnly ? sizeof(uint8_t) : sizeof(uint32_t), VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT};
        default:
            return {0, 0};
    }
}

void will_engine::vk_helpers::saveImage(renderer::ResourceManager& resourceManager, const renderer::ImmediateSubmitter& immediate,
                                        const renderer::AllocatedImage& image, VkImageLayout imageLayout,
                                        ImageFormat format, const std::string& savePath, bool saveStencilOnly)
{
    const FormatInfo info = getFormatInfo(format, saveStencilOnly);
    const VkImageAspectFlags aspectMask = info.aspectMask;

    VkImageAspectFlags subresourceAspect = aspectMask;
    // This will look different if `VK_KHR_separate_depth_stencil_layouts` is enabled
    if (info.aspectMask == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
        subresourceAspect = saveStencilOnly ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    const uint32_t texelCount = image.imageExtent.width * image.imageExtent.height;
    const size_t dataSize = texelCount * info.bytesPerPixel;
    renderer::AllocatedBuffer receivingBuffer = resourceManager.createReceivingBuffer(dataSize);

    immediate.submit([&](VkCommandBuffer cmd) {
        VkBufferImageCopy bufferCopyRegion{};
        bufferCopyRegion.imageSubresource.aspectMask = subresourceAspect;
        bufferCopyRegion.imageSubresource.mipLevel = 0;
        bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent = image.imageExtent;
        bufferCopyRegion.bufferOffset = 0;
        bufferCopyRegion.bufferRowLength = 0;
        bufferCopyRegion.bufferImageHeight = 0;

        imageBarrier(cmd, image.image, imageLayout, VK_IMAGE_LAYOUT_GENERAL, aspectMask);
        vkCmdCopyImageToBuffer(cmd, image.image, VK_IMAGE_LAYOUT_GENERAL, receivingBuffer.buffer, 1, &bufferCopyRegion);
        imageBarrier(cmd, image.image, VK_IMAGE_LAYOUT_GENERAL, imageLayout, aspectMask);
    });


    void* data = receivingBuffer.info.pMappedData;
    std::vector<uint8_t> outputData(texelCount * 4);
    processImageData(data, outputData, texelCount, format, saveStencilOnly);

    stbi_write_png(savePath.c_str(), image.imageExtent.width, image.imageExtent.height, 4, outputData.data(), image.imageExtent.width * 4);

    resourceManager.destroyResourceImmediate(receivingBuffer);
}

void will_engine::vk_helpers::processImageData(void* sourceData, std::span<uint8_t> targetData, uint32_t pixelCount, ImageFormat format,
                                               bool stencilOnly)
{
    constexpr float powEight = 255.0f;

    switch (format) {
        case ImageFormat::RGBA32F:
        {
            const auto floatData = static_cast<float*>(sourceData);
            for (uint32_t i = 0; i < pixelCount; ++i) {
                for (uint32_t c = 0; c < 4; ++c) {
                    const float value = std::max(0.0f, std::min(1.0f, floatData[i * 4 + c]));
                    targetData[i * 4 + c] = static_cast<uint8_t>(value * powEight);
                }
            }
            break;
        }

        case ImageFormat::RGBA16F:
        {
            const auto halfData = static_cast<half_float::half*>(sourceData);
            for (uint32_t i = 0; i < pixelCount; ++i) {
                for (uint32_t c = 0; c < 4; ++c) {
                    float value = halfData[i * 4 + c];
                    value = std::max(0.0f, std::min(1.0f, value));
                    targetData[i * 4 + c] = static_cast<uint8_t>(value * powEight);
                }
            }
            break;
        }

        case ImageFormat::RGBA8:
        {
            const auto packedData = static_cast<uint32_t*>(sourceData);
            for (uint32_t i = 0; i < pixelCount; ++i) {
                const uint32_t pixel = packedData[i];
                targetData[i * 4] = (pixel & 0xFF);
                targetData[i * 4 + 1] = ((pixel >> 8) & 0xFF);
                targetData[i * 4 + 2] = ((pixel >> 16) & 0xFF);
                targetData[i * 4 + 3] = ((pixel >> 24) & 0xFF);
            }
            break;
        }
        case ImageFormat::RGBA8_UNORM:
        {
            const auto packedData = static_cast<uint32_t*>(sourceData);
            for (uint32_t i = 0; i < pixelCount; ++i) {
                const glm::vec4 pixel = glm::unpackUnorm4x8(packedData[i]);
                targetData[i * 4] = pixel.r * powEight;
                targetData[i * 4 + 1] = pixel.g * powEight;
                targetData[i * 4 + 2] = pixel.b * powEight;
                targetData[i * 4 + 3] = powEight;
            }
            break;
        }

        case ImageFormat::R32F:
        {
            const auto floatData = static_cast<float*>(sourceData);
            for (uint32_t i = 0; i < pixelCount; ++i) {
                const float depth = floatData[i];
                for (uint32_t c = 0; c < 3; ++c) {
                    targetData[i * 4 + c] = static_cast<uint8_t>(depth * powEight);
                }
                targetData[i * 4 + 3] = powEight;
            }
            break;
        }

        case ImageFormat::R16F:
        {
            const auto halfData = static_cast<half_float::half*>(sourceData);
            for (uint32_t i = 0; i < pixelCount; ++i) {
                const float depth = halfData[i];
                for (uint32_t c = 0; c < 3; ++c) {
                    targetData[i * 4 + c] = static_cast<uint8_t>(depth * powEight);
                }
                targetData[i * 4 + 3] = 255;
            }
            break;
        }

        case ImageFormat::R8UNORM:
        case ImageFormat::R8UINT:
        {
            const uint8_t* byteSource = static_cast<uint8_t*>(sourceData);
            for (uint32_t i = 0; i < pixelCount; ++i) {
                const uint8_t value = byteSource[i];
                for (uint32_t c = 0; c < 3; ++c) {
                    targetData[i * 4 + c] = value;
                }
                targetData[i * 4 + 3] = 255;
            }
            break;
        }
        case ImageFormat::A2R10G10B10_UNORM:
        {
            const uint32_t* byteSource = static_cast<uint32_t*>(sourceData);
            for (uint32_t i = 0; i < pixelCount; ++i) {
                const uint32_t value = byteSource[i];

                // Extract components according to A2R10G10B10 layout (AI BS)
                const float r = static_cast<float>((value >> 20) & 0x3FF) / 1023.0f;
                const float g = static_cast<float>((value >> 10) & 0x3FF) / 1023.0f;
                const float b = static_cast<float>(value & 0x3FF) / 1023.0f;
                //const float a = static_cast<float>((value >> 30) & 0x3) / 3.0f;

                targetData[i * 4 + 0] = static_cast<uint8_t>(r * powEight);
                targetData[i * 4 + 1] = static_cast<uint8_t>(g * powEight);
                targetData[i * 4 + 2] = static_cast<uint8_t>(b * powEight);
                //targetData[i * 4 + 3] = static_cast<uint8_t>(a * 255.0f);
                targetData[i * 4 + 3] = powEight;
            }
            break;
        }
        case ImageFormat::D32S8:
        {
            if (stencilOnly) {
                const uint8_t* byteSource = static_cast<uint8_t*>(sourceData);
                for (uint32_t i = 0; i < pixelCount; ++i) {
                    const uint8_t stencil = byteSource[i];
                    for (uint32_t c = 0; c < 3; ++c) {
                        targetData[i * 4 + c] = stencil * 255.0f;
                    }
                    targetData[i * 4 + 3] = 255;
                }
            }
            else {
                const auto depthData = static_cast<float*>(sourceData);
                for (uint32_t i = 0; i < pixelCount; ++i) {
                    const float depth = depthData[i];
                    for (uint32_t c = 0; c < 3; ++c) {
                        targetData[i * 4 + c] = static_cast<uint8_t>(depth * powEight);
                    }
                    targetData[i * 4 + 3] = 255;
                }
            }
            break;
        }
    }
}
