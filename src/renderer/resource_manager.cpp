//
// Created by William on 2024-12-13.
//

#include "resource_manager.h"

#include "glm/glm.hpp"
#include "glm/fwd.hpp"
#include "glm/gtc/packing.hpp"

#include "immediate_submitter.h"
#include "vk_descriptors.h"
#include "vk_helpers.h"
#include "vulkan_context.h"
#include "render_object/render_object_constants.h"
#include "descriptor_buffer/descriptor_buffer_uniform.h"


ResourceManager::ResourceManager(const VulkanContext& context, ImmediateSubmitter& immediate) : context(context), immediate(immediate)
{
    // white
    {
        const uint32_t white = packUnorm4x8(glm::vec4(1, 1, 1, 1));
        whiteImage = createImage(&white, 4, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
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
        errorCheckerboardImage = createImage(pixels.data(), 16 * 16 * 4, VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    }
    // nearest sampler
    {
        VkSamplerCreateInfo samplerInfo = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        vkCreateSampler(context.device, &samplerInfo, nullptr, &defaultSamplerNearest);
    }
    // linear sampler
    {
        VkSamplerCreateInfo samplerInfo = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        vkCreateSampler(context.device, &samplerInfo, nullptr, &defaultSamplerLinear);
    }
    // Empty (WIP/unused)
    {
        DescriptorLayoutBuilder layoutBuilder;
        emptyDescriptorSetLayout = layoutBuilder.build(context.device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, nullptr,
                                                       VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    }

    // Scene Data Layout
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        sceneDataLayout = layoutBuilder.build(context.device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, nullptr,
                                              VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }
    // Frustum Cull Layout
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        frustumCullLayout = layoutBuilder.build(context.device, VK_SHADER_STAGE_COMPUTE_BIT, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }

    // Render Object Addresses
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        addressesLayout = layoutBuilder.build(context.device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }

    // Render Object Textures
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_SAMPLER, will_engine::render_object_constants::MAX_SAMPLER_COUNT);
        layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, will_engine::render_object_constants::MAX_IMAGES_COUNT);
        texturesLayout = layoutBuilder.build(context.device, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }
    // Render Targets
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        layoutBuilder.addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        layoutBuilder.addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        layoutBuilder.addBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

        renderTargetsLayout = layoutBuilder.build(context.device, VK_SHADER_STAGE_COMPUTE_BIT, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }

}

ResourceManager::~ResourceManager()
{
    if (context.device == VK_NULL_HANDLE) { return; }

    destroyImage(whiteImage);
    destroyImage(errorCheckerboardImage);
    vkDestroySampler(context.device, defaultSamplerNearest, nullptr);
    vkDestroySampler(context.device, defaultSamplerLinear, nullptr);
    vkDestroyDescriptorSetLayout(context.device, emptyDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(context.device, sceneDataLayout, nullptr);
    vkDestroyDescriptorSetLayout(context.device, frustumCullLayout, nullptr);
    vkDestroyDescriptorSetLayout(context.device, addressesLayout, nullptr);
    vkDestroyDescriptorSetLayout(context.device, texturesLayout, nullptr);
    vkDestroyDescriptorSetLayout(context.device, renderTargetsLayout, nullptr);
}

AllocatedBuffer ResourceManager::createBuffer(const size_t allocSize, const VkBufferUsageFlags usage, const VmaMemoryUsage memoryUsage) const
{
    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memoryUsage;
    vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    vmaallocInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    AllocatedBuffer newBuffer{};

    VK_CHECK(vmaCreateBuffer(context.allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

    return newBuffer;
}

AllocatedBuffer ResourceManager::createHostSequentialBuffer(const size_t allocSize) const
{
    const VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = allocSize,
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    constexpr VmaAllocationCreateInfo allocInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
    };

    AllocatedBuffer newBuffer{};
    VK_CHECK(vmaCreateBuffer(context.allocator, &bufferInfo, &allocInfo,&newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

    return newBuffer;
}

AllocatedBuffer ResourceManager::createDeviceBuffer(const size_t allocSize, const VkBufferUsageFlags additionalUsages) const
{
    const VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = allocSize,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | additionalUsages,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    constexpr VmaAllocationCreateInfo allocInfo{
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    AllocatedBuffer newBuffer{};
    VK_CHECK(vmaCreateBuffer(context.allocator, &bufferInfo, &allocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

    return newBuffer;
}

AllocatedBuffer ResourceManager::createStagingBuffer(const size_t allocSize) const
{
    const VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = allocSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    constexpr VmaAllocationCreateInfo allocInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
    };

    AllocatedBuffer newBuffer{};
    VK_CHECK(vmaCreateBuffer(context.allocator, &bufferInfo, &allocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

    return newBuffer;
}

AllocatedBuffer ResourceManager::createReceivingBuffer(const size_t allocSize) const
{
    return createBuffer(allocSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU);
}

void ResourceManager::copyBuffer(const AllocatedBuffer& src, const AllocatedBuffer& dst, const VkDeviceSize size) const
{
    immediate.submit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{};
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = size;

        vkCmdCopyBuffer(cmd, src.buffer, dst.buffer, 1, &vertexCopy);
    });
}

void ResourceManager::copyBuffer(const AllocatedBuffer& src, const AllocatedBuffer& dst, const VkDeviceSize size, const VkDeviceSize offset) const
{
    immediate.submit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{};
        vertexCopy.dstOffset = offset;
        vertexCopy.srcOffset = offset;
        vertexCopy.size = size;

        vkCmdCopyBuffer(cmd, src.buffer, dst.buffer, 1, &vertexCopy);
    });
}


VkDeviceAddress ResourceManager::getBufferAddress(const AllocatedBuffer& buffer) const
{
    VkBufferDeviceAddressInfo addressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR};
    addressInfo.buffer = buffer.buffer;
    const VkDeviceAddress srcPtr = vkGetBufferDeviceAddress(context.device, &addressInfo);
    return srcPtr;
}

void ResourceManager::destroyBuffer(AllocatedBuffer& buffer) const
{
    if (buffer.buffer == VK_NULL_HANDLE) { return; }
    vmaDestroyBuffer(context.allocator, buffer.buffer, buffer.allocation);
    buffer.buffer = VK_NULL_HANDLE;
}

VkSampler ResourceManager::createSampler(const VkSamplerCreateInfo& createInfo) const
{
    VkSampler newSampler;
    vkCreateSampler(context.device, &createInfo, nullptr, &newSampler);
    return newSampler;
}

AllocatedImage ResourceManager::createImage(const VkExtent3D size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped) const
{
    AllocatedImage newImage{};
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo imgInfo = vk_helpers::imageCreateInfo(format, usage, size);
    if (mipmapped) {
        imgInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocinfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    VK_CHECK(vmaCreateImage(context.allocator, &imgInfo, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

    const VkImageAspectFlags aspectFlag = format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    // build a image-view for the image
    VkImageViewCreateInfo view_info = vk_helpers::imageviewCreateInfo(format, newImage.image, aspectFlag);
    view_info.subresourceRange.levelCount = imgInfo.mipLevels;

    VK_CHECK(vkCreateImageView(context.device, &view_info, nullptr, &newImage.imageView));

    return newImage;
}

AllocatedImage ResourceManager::createImage(const void* data, const size_t dataSize, const VkExtent3D size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped) const
{
    const size_t data_size = dataSize;
    AllocatedBuffer uploadbuffer = createBuffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    memcpy(uploadbuffer.info.pMappedData, data, data_size);

    const AllocatedImage newImage = createImage(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

    immediate.submit([&](VkCommandBuffer cmd) {
        vk_helpers::transitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

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
        vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        if (mipmapped) {
            vk_helpers::generateMipmaps(cmd, newImage.image, VkExtent2D{newImage.imageExtent.width, newImage.imageExtent.height});
        } else {
            vk_helpers::transitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
        }
    });

    destroyBuffer(uploadbuffer);

    return newImage;
}

AllocatedImage ResourceManager::createCubemap(const VkExtent3D size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped) const
{
    AllocatedImage newImage{};
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo img_info = vk_helpers::cubemapCreateInfo(format, usage, size);
    if (mipmapped) {
        img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }
    // always allocate images on dedicated GPU memory
    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocinfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VK_CHECK(vmaCreateImage(context.allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

    VkImageViewCreateInfo view_info = vk_helpers::cubemapViewCreateInfo(format, newImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
    view_info.subresourceRange.levelCount = img_info.mipLevels;

    VK_CHECK(vkCreateImageView(context.device, &view_info, nullptr, &newImage.imageView));

    return newImage;
}

void ResourceManager::destroyImage(const AllocatedImage& img) const
{
    vkDestroyImageView(context.device, img.imageView, nullptr);
    vmaDestroyImage(context.allocator, img.image, img.allocation);
}

void ResourceManager::destroySampler(const VkSampler& sampler) const
{
    vkDestroySampler(context.device, sampler, nullptr);
}

DescriptorBufferSampler ResourceManager::createDescriptorBufferSampler(VkDescriptorSetLayout layout, int32_t maxObjectCount) const
{
    return DescriptorBufferSampler(context, layout, maxObjectCount);
}

int32_t ResourceManager::setupDescriptorBufferSampler(DescriptorBufferSampler& descriptorBuffer, const std::vector<will_engine::DescriptorImageData>& imageBuffers, const int index) const
{
    return descriptorBuffer.setupData(context.device, imageBuffers, index);
}

DescriptorBufferUniform ResourceManager::createDescriptorBufferUniform(VkDescriptorSetLayout layout, int32_t maxObjectCount) const
{
    return DescriptorBufferUniform(context, layout, maxObjectCount);
}

int32_t ResourceManager::setupDescriptorBufferUniform(DescriptorBufferUniform& descriptorBuffer, const std::vector<will_engine::DescriptorUniformData>& uniformBuffers, const int index) const
{
    return descriptorBuffer.setupData(context.device, uniformBuffers, index);
}

void ResourceManager::destroyDescriptorBuffer(DescriptorBuffer& descriptorBuffer) const
{
    descriptorBuffer.destroy(context.allocator);
}

VkShaderModule ResourceManager::createShaderModule(const std::filesystem::path& path) const
{
    VkShaderModule shader;
    const bool res = vk_helpers::loadShaderModule(path.string().c_str(), context.device, &shader);
    if (!res) {
        const std::string message = fmt::format("Error when building shader {}", path.filename().string());
        throw std::runtime_error(message);
    }

    return shader;
}

void ResourceManager::destroyShaderModule(VkShaderModule& shaderModule) const
{
    vkDestroyShaderModule(context.device, shaderModule, nullptr);
    shaderModule = VK_NULL_HANDLE;
}

VkPipelineLayout ResourceManager::createPipelineLayout(const VkPipelineLayoutCreateInfo& createInfo) const
{
    VkPipelineLayout pipelineLayout;
    VK_CHECK(vkCreatePipelineLayout(context.device, &createInfo, nullptr, &pipelineLayout));
    return pipelineLayout;
}


void ResourceManager::destroyPipelineLayout(VkPipelineLayout& pipelineLayout) const
{
    if (pipelineLayout == VK_NULL_HANDLE) { return; }
    vkDestroyPipelineLayout(context.device, pipelineLayout, nullptr);
    pipelineLayout = VK_NULL_HANDLE;
}

VkPipeline ResourceManager::createRenderPipeline(PipelineBuilder& builder) const
{
    const VkPipeline pipeline = builder.buildPipeline(context.device, VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    return pipeline;
}

VkPipeline ResourceManager::createComputePipeline(const VkComputePipelineCreateInfo& pipelineInfo) const
{
    VkPipeline computePipeline;
    vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline);
    return computePipeline;
}

void ResourceManager::destroyPipeline(VkPipeline& pipeline) const
{
    if (pipeline == VK_NULL_HANDLE) { return; }
    vkDestroyPipeline(context.device, pipeline, nullptr);
    pipeline = VK_NULL_HANDLE;
}

VkDescriptorSetLayout ResourceManager::createDescriptorSetLayout(DescriptorLayoutBuilder& layoutBuilder, const VkShaderStageFlagBits shaderStageFlags, const VkDescriptorSetLayoutCreateFlagBits layoutCreateFlags) const
{
    return layoutBuilder.build(context.device, shaderStageFlags, nullptr, layoutCreateFlags);
}

void ResourceManager::destroyDescriptorSetLayout(VkDescriptorSetLayout& descriptorSetLayout) const
{
    if (descriptorSetLayout == VK_NULL_HANDLE) { return; }
    vkDestroyDescriptorSetLayout(context.device, descriptorSetLayout, nullptr);
    descriptorSetLayout = VK_NULL_HANDLE;
}

VkImageView ResourceManager::createImageView(const VkImageViewCreateInfo& viewInfo) const
{
    VkImageView imageView;
    VK_CHECK(vkCreateImageView(context.device, &viewInfo, nullptr, &imageView));
    return imageView;
}

void ResourceManager::destroyImageView(VkImageView& imageView) const
{
    if (imageView == VK_NULL_HANDLE) { return; }
    vkDestroyImageView(context.device, imageView, nullptr);
    imageView = VK_NULL_HANDLE;
}
