//
// Created by William on 2024-12-13.
//

#include "resource_manager.h"

#include <array>
#include <fstream>

#include "glm/glm.hpp"
#include "glm/fwd.hpp"
#include "glm/gtc/packing.hpp"

#include "immediate_submitter.h"
#include "vk_descriptors.h"
#include "vk_helpers.h"
#include "vulkan_context.h"
#include "assets/render_object/render_object_constants.h"
#include "resources/buffer.h"
#include "resources/descriptor_set_layout.h"
#include "resources/image.h"
#include "shaderc/shaderc.hpp"
#include "terrain/terrain_constants.h"

namespace will_engine::renderer
{
ResourceManager::ResourceManager(VulkanContext& context, ImmediateSubmitter& immediate)
    : context(context), immediate(immediate)
{
    // white
    {
        const uint32_t white = packUnorm4x8(glm::vec4(1, 1, 1, 1));
        whiteImage = createImageFromData(&white, 4, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    }
    // checkerboard
    {
        const uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
        const uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
        std::array<uint32_t, 16 * 16> pixels{}; //for 16x16 checkerboard texture
        for (int x = 0; x < 16; x++) {
            for (int y = 0; y < 16; y++) {
                pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
            }
        }
        errorCheckerboardImage = createImageFromData(pixels.data(), 16 * 16 * 4, VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM,
                                                     VK_IMAGE_USAGE_SAMPLED_BIT);
    }
    // nearest sampler
    {
        VkSamplerCreateInfo samplerInfo = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        defaultSamplerNearest = createResource<Sampler>(samplerInfo);
    }
    // linear sampler
    {
        VkSamplerCreateInfo samplerInfo = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        defaultSamplerLinear = createResource<Sampler>(samplerInfo);
    }
    // linear sampler mipmapped
    {
        VkSamplerCreateInfo samplerInfo = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 8;
        defaultSamplerMipMappedLinear = createResource<Sampler>(samplerInfo);
    }

    // Empty (WIP/unused)
    {
        DescriptorLayoutBuilder layoutBuilder;
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = layoutBuilder.build(
            static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT),
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );
        emptyDescriptorSetLayout = createResource<DescriptorSetLayout>(layoutCreateInfo);
    }

    // Scene Data Layout
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = layoutBuilder.build(
            static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT |
                                               VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );
        sceneDataLayout = createResource<DescriptorSetLayout>(layoutCreateInfo);
    }
    // Frustum Cull Layout
    {
        DescriptorLayoutBuilder layoutBuilder{1};
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = layoutBuilder.build(
            VK_SHADER_STAGE_COMPUTE_BIT,
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );
        frustumCullLayout = createResource<DescriptorSetLayout>(layoutCreateInfo);
    }

    // Render Object Addresses
    {
        DescriptorLayoutBuilder layoutBuilder{1};
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = layoutBuilder.build(
            static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT),
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );
        addressesLayout = createResource<DescriptorSetLayout>(layoutCreateInfo);
    }

    // Render Object Textures
    {
        DescriptorLayoutBuilder layoutBuilder{2};
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_SAMPLER, render_object_constants::MAX_SAMPLER_COUNT);
        layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, render_object_constants::MAX_IMAGES_COUNT);
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = layoutBuilder.build(
            VK_SHADER_STAGE_FRAGMENT_BIT,
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );
        texturesLayout = createResource<DescriptorSetLayout>(layoutCreateInfo);
    }
    // Render Targets
    {
        DescriptorLayoutBuilder layoutBuilder{8};
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // Normals
        layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // Albedo
        layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // PBR
        layoutBuilder.addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // Depth
        layoutBuilder.addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // Velocity
        layoutBuilder.addBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // AO
        layoutBuilder.addBinding(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // Screen Space Contact Shadows
        layoutBuilder.addBinding(7, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // Output
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = layoutBuilder.build(
            VK_SHADER_STAGE_COMPUTE_BIT,
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );
        renderTargetsLayout = createResource<DescriptorSetLayout>(layoutCreateInfo);
    }

    // Terrain Textures
    {
        DescriptorLayoutBuilder layoutBuilder{1};
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, terrain::MAX_TERRAIN_TEXTURE_COUNT);
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = layoutBuilder.build(
            VK_SHADER_STAGE_FRAGMENT_BIT,
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );
        terrainTexturesLayout = createResource<DescriptorSetLayout>(layoutCreateInfo);
    }
    // Terrain Uniform
    {
        DescriptorLayoutBuilder layoutBuilder{1};
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = layoutBuilder.build(
            VK_SHADER_STAGE_FRAGMENT_BIT,
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );
        terrainUniformLayout = createResource<DescriptorSetLayout>(layoutCreateInfo);
    }

    for (int32_t i = 0; i < FRAME_OVERLAP; ++i) {
        destructionQueues[i].resources.reserve(100);
    }

    const VkCommandPoolCreateInfo poolInfo = vk_helpers::commandPoolCreateInfo(context.graphicsQueueFamily,
                                                                               VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    VK_CHECK(vkCreateCommandPool(context.device, &poolInfo, nullptr, &ktxTextureCommandPool));
    vulkanDeviceInfo = ktxVulkanDeviceInfo_Create(context.physicalDevice, context.device, context.graphicsQueue, ktxTextureCommandPool, nullptr);
}

ResourceManager::~ResourceManager()
{
    if (context.device == VK_NULL_HANDLE) { return; }

    destroyResource(std::move(whiteImage));
    destroyResource(std::move(errorCheckerboardImage));
    destroyResource(std::move(defaultSamplerNearest));
    destroyResource(std::move(defaultSamplerLinear));
    destroyResource(std::move(defaultSamplerMipMappedLinear));
    destroyResource(std::move(emptyDescriptorSetLayout));
    destroyResource(std::move(sceneDataLayout));
    destroyResource(std::move(frustumCullLayout));
    destroyResource(std::move(addressesLayout));
    destroyResource(std::move(texturesLayout));
    destroyResource(std::move(renderTargetsLayout));
    destroyResource(std::move(terrainTexturesLayout));
    destroyResource(std::move(terrainUniformLayout));

    flushDestructionQueue();

    ktxVulkanDeviceInfo_Destroy(vulkanDeviceInfo);
    vkDestroyCommandPool(context.device, ktxTextureCommandPool, nullptr);
}

void ResourceManager::update(const int32_t currentFrameOverlap)
{
    if (currentFrameOverlap < 0 || currentFrameOverlap >= FRAME_OVERLAP) {
        return;
    }

    destructionQueues[currentFrameOverlap].flush();
    lastKnownFrameOverlap = currentFrameOverlap;
}

void ResourceManager::flushDestructionQueue()
{
    for (int32_t i = 0; i < FRAME_OVERLAP; ++i) {
        destructionQueues[i].flush();
    }
}

ImageFormatProperties ResourceManager::getPhysicalDeviceImageFormatProperties(
    const VkFormat format, const VkImageUsageFlags usageFlags) const
{
    VkImageFormatProperties formatProperties;
    const VkResult result = vkGetPhysicalDeviceImageFormatProperties(context.physicalDevice, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                                                                     usageFlags, 0, &formatProperties);
    if (result == VK_ERROR_FORMAT_NOT_SUPPORTED) {
        return {VK_ERROR_FORMAT_NOT_SUPPORTED, {}};
    }

    return {result, formatProperties};
}

void ResourceManager::copyBufferImmediate(const VkBuffer src, const VkBuffer dst,
                                          const VkDeviceSize size) const
{
    copyBufferImmediate(src, dst, size, 0);
}

void ResourceManager::copyBufferImmediate(const VkBuffer src, const VkBuffer dst, const VkDeviceSize size,
                                          const VkDeviceSize offset) const
{
    immediate.submit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{};
        vertexCopy.dstOffset = offset;
        vertexCopy.srcOffset = offset;
        vertexCopy.size = size;

        vkCmdCopyBuffer(cmd, src, dst, 1, &vertexCopy);
    });
}

void ResourceManager::copyBufferImmediate(const std::span<BufferCopyInfo> bufferCopyInfos) const
{
    immediate.submit([bufferCopyInfos](const VkCommandBuffer cmd) {
        for (const BufferCopyInfo bufferCopyInfo : bufferCopyInfos) {
            VkBufferCopy bufferCopy{};
            bufferCopy.dstOffset = bufferCopyInfo.dstOffset;
            bufferCopy.srcOffset = bufferCopyInfo.srcOffset;
            bufferCopy.size = bufferCopyInfo.size;

            vkCmdCopyBuffer(cmd, bufferCopyInfo.src, bufferCopyInfo.dst, 1, &bufferCopy);
        }
    });
}


VkDeviceAddress ResourceManager::getBufferAddress(const Buffer& buffer) const
{
    VkBufferDeviceAddressInfo addressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR};
    addressInfo.buffer = buffer.buffer;
    const VkDeviceAddress srcPtr = vkGetBufferDeviceAddress(context.device, &addressInfo);
    return srcPtr;
}

VkDeviceAddress ResourceManager::getBufferAddress(VkBuffer buffer) const
{
    VkBufferDeviceAddressInfo addressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR};
    addressInfo.buffer = buffer;
    const VkDeviceAddress srcPtr = vkGetBufferDeviceAddress(context.device, &addressInfo);
    return srcPtr;
}

ImageResourcePtr ResourceManager::createCubemapImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    VkImageCreateInfo createInfo = vk_helpers::imageCreateInfo(format, usage, size);
    if (mipmapped) {
        createInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }
    createInfo.arrayLayers = 6;
    createInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    constexpr VmaAllocationCreateInfo allocInfo{
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };

    VkImageViewCreateInfo viewInfo = vk_helpers::cubemapViewCreateInfo(format, VK_NULL_HANDLE, VK_IMAGE_ASPECT_COLOR_BIT);
    viewInfo.subresourceRange.levelCount = createInfo.mipLevels;

    return createResource<Image>(createInfo, allocInfo, viewInfo);
}

ImageResourcePtr ResourceManager::createImageFromData(const void* data, size_t dataSize, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                                                      bool mipmapped)
{
    const size_t data_size = dataSize;

    BufferPtr uploadBuffer = createResource<Buffer>(BufferType::Staging, data_size);
    memcpy(uploadBuffer->info.pMappedData, data, data_size);

    ImageResourcePtr newImage = createResource<Image>(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

    immediate.submit([&](VkCommandBuffer cmd) {
        vk_helpers::imageBarrier(cmd, newImage.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = size;

        // copy the buffer into the image
        vkCmdCopyBufferToImage(cmd, uploadBuffer->buffer, newImage->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        if (mipmapped) {
            vk_helpers::generateMipmaps(cmd, newImage->image, VkExtent2D{newImage->imageExtent.width, newImage->imageExtent.height});
        }
        else {
            vk_helpers::imageBarrier(cmd, newImage.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                     VK_IMAGE_ASPECT_COLOR_BIT);
        }
        newImage->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    });

    destroyResourceImmediate(std::move(uploadBuffer));

    return newImage;
}
}
