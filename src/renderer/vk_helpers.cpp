//
// Created by William on 8/11/2024.
//

#include "vk_helpers.h"

#include <filesystem>
#include <stb_image.h>
#include <stb_image_write.h>

#include "../../extern/half/half/half.hpp"
#include "../core/engine.h"

VkImageCreateInfo vk_helpers::imageCreateInfo(const VkFormat format, const VkImageUsageFlags usageFlags, const VkExtent3D extent)
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

VkImageCreateInfo vk_helpers::cubemapCreateInfo(const VkFormat format, const VkImageUsageFlags usageFlags, const VkExtent3D extent)
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

VkImageViewCreateInfo vk_helpers::imageviewCreateInfo(const VkFormat format, const VkImage image, const VkImageAspectFlags aspectFlags)
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

VkImageViewCreateInfo vk_helpers::cubemapViewCreateInfo(const VkFormat format, const VkImage image, const VkImageAspectFlags aspectFlags)
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

VkImageSubresourceRange vk_helpers::imageSubresourceRange(const VkImageAspectFlags aspectMask)
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


VkCommandPoolCreateInfo vk_helpers::commandPoolCreateInfo(uint32_t queueFamilyIndex, const VkCommandPoolCreateFlags flags /*= 0*/)
{
    VkCommandPoolCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = flags;
    return info;
}

// ReSharper disable once CppParameterMayBeConst
VkCommandBufferAllocateInfo vk_helpers::commandBufferAllocateInfo(VkCommandPool pool, const uint32_t count /*= 1*/)
{
    VkCommandBufferAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.pNext = nullptr;

    info.commandPool = pool;
    info.commandBufferCount = count;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    return info;
}

VkCommandBufferBeginInfo vk_helpers::commandBufferBeginInfo(const VkCommandBufferUsageFlags flags)
{
    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.pNext = nullptr;

    info.pInheritanceInfo = nullptr;
    info.flags = flags;
    return info;
}

// ReSharper disable once CppParameterMayBeConst
VkCommandBufferSubmitInfo vk_helpers::commandBufferSubmitInfo(VkCommandBuffer cmd)
{
    VkCommandBufferSubmitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    info.pNext = nullptr;
    info.commandBuffer = cmd;
    info.deviceMask = 0;

    return info;
}


VkFenceCreateInfo vk_helpers::fenceCreateInfo(const VkFenceCreateFlags flags /*= 0*/)
{
    VkFenceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = flags;
    return info;
}

VkSemaphoreCreateInfo vk_helpers::semaphoreCreateInfo(const VkSemaphoreCreateFlags flags /*= 0*/)
{
    VkSemaphoreCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = flags;
    return info;
}

// ReSharper disable once CppParameterMayBeConst
VkSemaphoreSubmitInfo vk_helpers::semaphoreSubmitInfo(const VkPipelineStageFlags2 stageMask, VkSemaphore semaphore)
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
VkRenderingAttachmentInfo vk_helpers::attachmentInfo(VkImageView view, const VkClearValue* clear, const VkImageLayout layout)
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

VkRenderingInfo vk_helpers::renderingInfo(const VkExtent2D renderExtent, const VkRenderingAttachmentInfo* colorAttachment, const VkRenderingAttachmentInfo* depthAttachment)
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

VkSubmitInfo2 vk_helpers::submitInfo(const VkCommandBufferSubmitInfo* cmd, const VkSemaphoreSubmitInfo* signalSemaphoreInfo, const VkSemaphoreSubmitInfo* waitSemaphoreInfo)
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
VkDeviceAddress vk_helpers::getDeviceAddress(VkDevice device, VkBuffer buffer)
{
    VkBufferDeviceAddressInfo deviceAdressInfo{};
    deviceAdressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    deviceAdressInfo.buffer = buffer;
    const uint64_t address = vkGetBufferDeviceAddress(device, &deviceAdressInfo);

    return address;
}


VkDeviceSize vk_helpers::getAlignedSize(const VkDeviceSize value, VkDeviceSize alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}


void vk_helpers::transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout targetLayout, VkImageAspectFlags aspectFlags)
{
    VkImageMemoryBarrier2 imageBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    imageBarrier.pNext = nullptr;

    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

    imageBarrier.oldLayout = currentLayout;
    imageBarrier.newLayout = targetLayout;

    const VkImageAspectFlags aspectMask = aspectFlags;

    imageBarrier.subresourceRange = imageSubresourceRange(aspectMask);
    imageBarrier.image = image;

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;

    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);
}

void vk_helpers::transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageAspectFlags aspectMask,
                                 VkImageLayout targetLayout)
{
    VkImageMemoryBarrier2 imageBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    imageBarrier.pNext = nullptr;

    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

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

void vk_helpers::copyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize)
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

void vk_helpers::copyDepthToDepth(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize)
{
    VkImageBlit2 blitRegion{.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr};

    blitRegion.srcOffsets[1].x = srcSize.width;
    blitRegion.srcOffsets[1].y = srcSize.height;
    blitRegion.srcOffsets[1].z = 1;

    blitRegion.dstOffsets[1].x = dstSize.width;
    blitRegion.dstOffsets[1].y = dstSize.height;
    blitRegion.dstOffsets[1].z = 1;

    blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcSubresource.layerCount = 1;
    blitRegion.srcSubresource.mipLevel = 0;

    blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    blitRegion.dstSubresource.baseArrayLayer = 0;
    blitRegion.dstSubresource.layerCount = 1;
    blitRegion.dstSubresource.mipLevel = 0;

    VkBlitImageInfo2 blitInfo{.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr};
    blitInfo.dstImage = destination;
    blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blitInfo.srcImage = source;
    blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blitInfo.filter = VK_FILTER_NEAREST;
    blitInfo.regionCount = 1;
    blitInfo.pRegions = &blitRegion;

    vkCmdBlitImage2(cmd, &blitInfo);
}

void vk_helpers::generateMipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize)
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
    transitionImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
}

VkPipelineLayoutCreateInfo vk_helpers::pipelineLayoutCreateInfo()
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

VkPipelineShaderStageCreateInfo vk_helpers::pipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule shaderModule, const char* entry)
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

bool vk_helpers::loadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule)
{
    const std::filesystem::path shaderPath(filePath);

    // open the file. With cursor at the end
    std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);


    if (!file.is_open()) {
        return false;
    }

    // find what the size of the file is by looking up the location of the cursor
    // because the cursor is at the end, it gives the size directly in bytes
    const size_t fileSize = (size_t) file.tellg();

    // spirv expects the buffer to be on uint32, so make sure to reserve a int
    // vector big enough for the entire file
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    // put file cursor at beginning
    file.seekg(0);

    // load the entire file into the buffer
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

    // now that the file is loaded into the buffer, we can close it
    file.close();

    // create a new shader module, using the buffer we loaded
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;

    // codeSize has to be in bytes, so multply the ints in the buffer by size of
    // int to know the real size of the buffer
    createInfo.codeSize = buffer.size() * sizeof(uint32_t);
    createInfo.pCode = buffer.data();

    // check that the creation goes well.
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        return false;
    }
    *outShaderModule = shaderModule;
    return true;
}


VkFilter vk_helpers::extractFilter(const fastgltf::Filter filter)
{
    switch (filter) {
        // nearest samplers
        case fastgltf::Filter::Nearest:
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::NearestMipMapLinear:
            return VK_FILTER_NEAREST;

        // linear samplers
        case fastgltf::Filter::Linear:
        case fastgltf::Filter::LinearMipMapNearest:
        case fastgltf::Filter::LinearMipMapLinear:
        default:
            return VK_FILTER_LINEAR;
    }
}

VkSamplerMipmapMode vk_helpers::extractMipmapMode(const fastgltf::Filter filter)
{
    switch (filter) {
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::LinearMipMapNearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;

        case fastgltf::Filter::NearestMipMapLinear:
        case fastgltf::Filter::LinearMipMapLinear:
        default:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

std::optional<AllocatedImage> vk_helpers::loadImage(const Engine* engine, const fastgltf::Asset& asset, const fastgltf::Image& image,
                                                    const std::filesystem::path& parentFolder)
{
    AllocatedImage newImage{};

    int width{}, height{}, nrChannels{};

    std::visit(
        fastgltf::visitor{
            [](auto& arg) {},
            [&](const fastgltf::sources::URI& fileName) {
                assert(fileName.fileByteOffset == 0); // We don't support offsets with stbi.
                assert(fileName.uri.isLocalPath()); // We're only capable of loading
                // local files.
                const std::wstring widePath(fileName.uri.path().begin(), fileName.uri.path().end());
                const std::filesystem::path fullPath = parentFolder / widePath;

                //std::string extension = getFileExtension(fullpath);
                /*if (isKTXFile(extension)) {
                    ktxTexture* kTexture;
                    KTX_error_code ktxresult;

                    ktxresult = ktxTexture_CreateFromNamedFile(
                        fullpath.c_str(),
                        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                        &kTexture);


                    if (ktxresult == KTX_SUCCESS) {
                        VkImageFormatProperties formatProperties;
                        VkResult result = vkGetPhysicalDeviceImageFormatProperties(engine->_physicalDevice
                            , ktxTexture_GetVkFormat(kTexture), VK_IMAGE_TYPE_2D
                            , VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, 0, &formatProperties);
                        if (result == VK_ERROR_FORMAT_NOT_SUPPORTED) {
                            fmt::print("Image found with format not supported\n");
                            VkExtent3D imagesize;
                            imagesize.width = 1;
                            imagesize.height = 1;
                            imagesize.depth = 1;
                            unsigned char data[4] = { 255, 0, 255, 1 };
                            newImage = engine->_resourceConstructor->create_image(data, 4, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false);
                        }
                        else {
                            unsigned char* data = (unsigned char*)ktxTexture_GetData(kTexture);
                            VkExtent3D imageExtents{};
                            imageExtents.width = kTexture->baseWidth;
                            imageExtents.height = kTexture->baseHeight;
                            imageExtents.depth = 1;
                            newImage = engine->_resourceConstructor->create_image(data, kTexture->dataSize, imageExtents, ktxTexture_GetVkFormat(kTexture), VK_IMAGE_USAGE_SAMPLED_BIT, false);
                        }

                    }

                    ktxTexture_Destroy(kTexture);
                }*/

                // ReSharper disable once CppTooWideScope
                unsigned char* data = stbi_load(fullPath.string().c_str(), &width, &height, &nrChannels, 4);
                if (data) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;
                    const size_t size = width * height * 4;
                    newImage = engine->createImage(data, size, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);

                    stbi_image_free(data);
                }
            },
            [&](const fastgltf::sources::Array& vector) {
                // ReSharper disable once CppTooWideScope
                unsigned char* data = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(vector.bytes.data()),
                                                            static_cast<int>(vector.bytes.size()), &width, &height, &nrChannels, 4);
                if (data) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;
                    const size_t size = width * height * 4;
                    newImage = engine->createImage(data, size, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);

                    stbi_image_free(data);
                }
            },
            [&](const fastgltf::sources::BufferView& view) {
                const fastgltf::BufferView& bufferView = asset.bufferViews[view.bufferViewIndex];
                const fastgltf::Buffer& buffer = asset.buffers[bufferView.bufferIndex];
                // We only care about VectorWithMime here, because we
                // specify LoadExternalBuffers, meaning all buffers
                // are already loaded into a vector.
                std::visit(fastgltf::visitor{
                               [](auto& arg) {},
                               [&](const fastgltf::sources::Array& vector) {
                                   // ReSharper disable once CppTooWideScope
                                   unsigned char* data = stbi_load_from_memory(
                                       reinterpret_cast<const stbi_uc*>(vector.bytes.data() + bufferView.byteOffset),
                                       static_cast<int>(bufferView.byteLength), &width, &height, &nrChannels, 4);
                                   if (data) {
                                       VkExtent3D imagesize;
                                       imagesize.width = width;
                                       imagesize.height = height;
                                       imagesize.depth = 1;
                                       const size_t size = width * height * 4;
                                       newImage = engine->createImage(data, size, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);
                                       stbi_image_free(data);
                                   }
                               }
                           }, buffer.data);
            }
        }, image.data);

    // if any of the attempts to load the data failed, we havent written the image
    // so handle is null
    if (newImage.image == VK_NULL_HANDLE) {
        fmt::print("Image failed to load: {}\n", image.name.c_str());
        return {};
    } else {
        return newImage;
    }
}

void vk_helpers::loadTexture(const fastgltf::Optional<fastgltf::TextureInfo>& texture, const fastgltf::Asset& gltf, int& imageIndex,
                             int& samplerIndex, const uint32_t imageOffset, const uint32_t samplerOffset)
{
    if (!texture.has_value()) {
        return;
    }

    const size_t textureIndex = texture.value().textureIndex;
    if (gltf.textures[textureIndex].imageIndex.has_value()) {
        imageIndex = gltf.textures[textureIndex].imageIndex.value() + imageOffset;
    }

    if (gltf.textures[textureIndex].samplerIndex.has_value()) {
        samplerIndex = gltf.textures[textureIndex].samplerIndex.value() + samplerOffset;
    }
}

void vk_helpers::saveImageRGBA32F(const Engine* engine, const AllocatedImage& image, const VkImageLayout imageLayout,
                                  const VkImageAspectFlags aspectFlag,
                                  const char* savePath, const bool overrideAlpha)
{
    constexpr int channelCount = 4;
    const size_t dataSize = image.imageExtent.width * image.imageExtent.height * channelCount * sizeof(float);
    AllocatedBuffer receivingBuffer = engine->createReceivingBuffer(dataSize);

    engine->immediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferImageCopy bufferCopyRegion{};
        bufferCopyRegion.imageSubresource.aspectMask = aspectFlag;
        bufferCopyRegion.imageSubresource.mipLevel = 0;
        bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent = image.imageExtent;
        bufferCopyRegion.bufferOffset = 0;
        bufferCopyRegion.bufferRowLength = 0;
        bufferCopyRegion.bufferImageHeight = 0;

        vk_helpers::transitionImage(cmd, image.image, imageLayout, aspectFlag, VK_IMAGE_LAYOUT_GENERAL);

        vkCmdCopyImageToBuffer(cmd, image.image, VK_IMAGE_LAYOUT_GENERAL, receivingBuffer.buffer, 1, &bufferCopyRegion);

        vk_helpers::transitionImage(cmd, image.image, VK_IMAGE_LAYOUT_GENERAL, imageLayout, VK_IMAGE_ASPECT_COLOR_BIT);
    });

    void* data = receivingBuffer.info.pMappedData;
    const auto imageData = static_cast<float*>(data);

    const auto byteImageData = new uint8_t[image.imageExtent.width * image.imageExtent.height * 4];
    const auto powEight = static_cast<float>(pow(2, 8) - 1);
    for (size_t i = 0; i < image.imageExtent.width * image.imageExtent.height; ++i) {
        for (int j = 0; j < channelCount; j++) {
            const auto value = static_cast<uint8_t>(imageData[i * channelCount + j] * powEight);
            byteImageData[i * 4 + j] = value;
        }

        if (overrideAlpha) {
            byteImageData[i * 4 + 3] = 255;
        }
    }

    stbi_write_png(savePath, image.imageExtent.width, image.imageExtent.height, 4, byteImageData, image.imageExtent.width * 4);

    delete[] byteImageData;
    engine->destroyBuffer(receivingBuffer);
}

void vk_helpers::saveImageRGBA16SFLOAT(const Engine* engine, const AllocatedImage& image, VkImageLayout imageLayout, VkImageAspectFlags aspectFlag,
                                       const char* savePath, const bool overrideAlpha)
{
    using half_float::half;
    constexpr int channelCount = 4;
    const size_t dataSize = image.imageExtent.width * image.imageExtent.height * channelCount * sizeof(half);
    AllocatedBuffer receivingBuffer = engine->createReceivingBuffer(dataSize);

    engine->immediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferImageCopy bufferCopyRegion{};
        bufferCopyRegion.imageSubresource.aspectMask = aspectFlag;
        bufferCopyRegion.imageSubresource.mipLevel = 0;
        bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent = image.imageExtent;
        bufferCopyRegion.bufferOffset = 0;
        bufferCopyRegion.bufferRowLength = 0;
        bufferCopyRegion.bufferImageHeight = 0;

        vk_helpers::transitionImage(cmd, image.image, imageLayout, aspectFlag, VK_IMAGE_LAYOUT_GENERAL);

        vkCmdCopyImageToBuffer(cmd, image.image, VK_IMAGE_LAYOUT_GENERAL, receivingBuffer.buffer, 1, &bufferCopyRegion);

        vk_helpers::transitionImage(cmd, image.image, VK_IMAGE_LAYOUT_GENERAL, imageLayout, VK_IMAGE_ASPECT_COLOR_BIT);
    });

    void* data = receivingBuffer.info.pMappedData;
    const auto imageData = static_cast<half*>(data);

    const auto byteImageData = new uint8_t[image.imageExtent.width * image.imageExtent.height * 4];
    const auto powEight = static_cast<half>(powf(2, 8) - 1);
    for (size_t i = 0; i < image.imageExtent.width * image.imageExtent.height; ++i) {
        for (int j = 0; j < channelCount; j++) {
            const auto value = static_cast<uint8_t>(imageData[i * channelCount + j] * powEight);
            byteImageData[i * 4 + j] = value;
        }

        if (overrideAlpha) {
            byteImageData[i * 4 + 3] = 255;
        }
    }

    stbi_write_png(savePath, image.imageExtent.width, image.imageExtent.height, 4, byteImageData, image.imageExtent.width * 4);

    delete[] byteImageData;
    engine->destroyBuffer(receivingBuffer);
}

void vk_helpers::savePacked32Bit(const Engine* engine, const AllocatedImage& image, VkImageLayout imageLayout, VkImageAspectFlags aspectFlag,
    const char* savePath, const std::function<glm::vec4(uint32_t)>& unpackingFunction)
{
    const size_t dataSize = image.imageExtent.width * image.imageExtent.height * sizeof(uint32_t);
    AllocatedBuffer receivingBuffer = engine->createReceivingBuffer(dataSize);

    engine->immediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferImageCopy bufferCopyRegion{};
        bufferCopyRegion.imageSubresource.aspectMask = aspectFlag;
        bufferCopyRegion.imageSubresource.mipLevel = 0;
        bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent = image.imageExtent;
        bufferCopyRegion.bufferOffset = 0;
        bufferCopyRegion.bufferRowLength = 0;
        bufferCopyRegion.bufferImageHeight = 0;

        vk_helpers::transitionImage(cmd, image.image, imageLayout, aspectFlag, VK_IMAGE_LAYOUT_GENERAL);

        vkCmdCopyImageToBuffer(cmd, image.image, VK_IMAGE_LAYOUT_GENERAL, receivingBuffer.buffer, 1, &bufferCopyRegion);

        vk_helpers::transitionImage(cmd, image.image, VK_IMAGE_LAYOUT_GENERAL, imageLayout, VK_IMAGE_ASPECT_COLOR_BIT);
    });

    void* data = receivingBuffer.info.pMappedData;
    const auto imageData = static_cast<uint32_t*>(data);

    const auto byteImageData = new uint8_t[image.imageExtent.width * image.imageExtent.height * 4];
    const auto powEight = powf(2, 8) - 1;
    for (size_t i = 0; i < image.imageExtent.width * image.imageExtent.height; ++i) {
        const glm::vec4 pixel = unpackingFunction(imageData[i]);

        byteImageData[i * 4] = static_cast<uint8_t>(pixel.r * powEight);
        byteImageData[i * 4 + 1] = static_cast<uint8_t>(pixel.g * powEight);
        byteImageData[i * 4 + 2] = static_cast<uint8_t>(pixel.b * powEight);
        byteImageData[i * 4 + 3] = static_cast<uint8_t>(pixel.a * powEight);
    }

    stbi_write_png(savePath, image.imageExtent.width, image.imageExtent.height, 4, byteImageData, image.imageExtent.width * 4);

    delete[] byteImageData;
    engine->destroyBuffer(receivingBuffer);
}

void vk_helpers::savePacked64Bit(const Engine* engine, const AllocatedImage& image, VkImageLayout imageLayout, VkImageAspectFlags aspectFlag,
    const char* savePath, const std::function<glm::vec4(uint64_t)>& unpackingFunction)
{
    const size_t dataSize = image.imageExtent.width * image.imageExtent.height * sizeof(uint64_t);
    AllocatedBuffer receivingBuffer = engine->createReceivingBuffer(dataSize);

    engine->immediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferImageCopy bufferCopyRegion{};
        bufferCopyRegion.imageSubresource.aspectMask = aspectFlag;
        bufferCopyRegion.imageSubresource.mipLevel = 0;
        bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent = image.imageExtent;
        bufferCopyRegion.bufferOffset = 0;
        bufferCopyRegion.bufferRowLength = 0;
        bufferCopyRegion.bufferImageHeight = 0;

        vk_helpers::transitionImage(cmd, image.image, imageLayout, aspectFlag, VK_IMAGE_LAYOUT_GENERAL);

        vkCmdCopyImageToBuffer(cmd, image.image, VK_IMAGE_LAYOUT_GENERAL, receivingBuffer.buffer, 1, &bufferCopyRegion);

        vk_helpers::transitionImage(cmd, image.image, VK_IMAGE_LAYOUT_GENERAL, imageLayout, VK_IMAGE_ASPECT_COLOR_BIT);
    });

    void* data = receivingBuffer.info.pMappedData;
    const auto imageData = static_cast<uint64_t*>(data);

    const auto byteImageData = new uint8_t[image.imageExtent.width * image.imageExtent.height * 4];
    const auto powEight = powf(2, 8) - 1;
    for (size_t i = 0; i < image.imageExtent.width * image.imageExtent.height; ++i) {
        const glm::vec4 pixel = unpackingFunction(imageData[i]);

        byteImageData[i * 4] = static_cast<uint8_t>(pixel.r * powEight);
        byteImageData[i * 4 + 1] = static_cast<uint8_t>(pixel.g * powEight);
        byteImageData[i * 4 + 2] = static_cast<uint8_t>(pixel.b * powEight);
        byteImageData[i * 4 + 3] = static_cast<uint8_t>(pixel.a * powEight);
    }

    stbi_write_png(savePath, image.imageExtent.width, image.imageExtent.height, 4, byteImageData, image.imageExtent.width * 4);

    delete[] byteImageData;
    engine->destroyBuffer(receivingBuffer);
}

void vk_helpers::saveImageR32F(const Engine* engine, const AllocatedImage& image, VkImageLayout imageLayout, VkImageAspectFlags aspectFlag,
                               const char* savePath, const std::function<float(float)>& valueTransform)
{
    const size_t dataSize = image.imageExtent.width * image.imageExtent.height * 1 * sizeof(float);
    AllocatedBuffer receivingBuffer = engine->createReceivingBuffer(dataSize);

    engine->immediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferImageCopy bufferCopyRegion{};
        bufferCopyRegion.imageSubresource.aspectMask = aspectFlag;
        bufferCopyRegion.imageSubresource.mipLevel = 0;
        bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent = image.imageExtent;
        bufferCopyRegion.bufferOffset = 0;
        bufferCopyRegion.bufferRowLength = 0;
        bufferCopyRegion.bufferImageHeight = 0;

        vk_helpers::transitionImage(cmd, image.image, imageLayout, aspectFlag, VK_IMAGE_LAYOUT_GENERAL);

        vkCmdCopyImageToBuffer(cmd, image.image, VK_IMAGE_LAYOUT_GENERAL, receivingBuffer.buffer, 1, &bufferCopyRegion);

        vk_helpers::transitionImage(cmd, image.image, VK_IMAGE_LAYOUT_GENERAL, imageLayout, aspectFlag);
    });

    void* data = receivingBuffer.info.pMappedData;
    const auto imageData = static_cast<float*>(data);

    const auto byteImageData = new uint8_t[image.imageExtent.width * image.imageExtent.height * 4];
    const auto powEight = static_cast<float>(pow(2, 8) - 1);
    for (size_t i = 0; i < image.imageExtent.width * image.imageExtent.height; ++i) {
        const float floatValue = valueTransform(imageData[i]);
        const auto value = static_cast<uint8_t>(floatValue * powEight);
        byteImageData[i * 4 + 0] = value;
        byteImageData[i * 4 + 1] = value;
        byteImageData[i * 4 + 2] = value;
        byteImageData[i * 4 + 3] = 255;
    }

    stbi_write_png(savePath, image.imageExtent.width, image.imageExtent.height, 4, byteImageData, image.imageExtent.width * 4);

    delete[] byteImageData;
    engine->destroyBuffer(receivingBuffer);
}
