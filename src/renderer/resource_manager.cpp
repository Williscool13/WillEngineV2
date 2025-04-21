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
#include "descriptor_buffer/descriptor_buffer_uniform.h"
#include "shaderc/shaderc.hpp"
#include "terrain/terrain_constants.h"


will_engine::ResourceManager::ResourceManager(const VulkanContext& context, ImmediateSubmitter& immediate) : context(context), immediate(immediate)
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
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(context.device, &samplerInfo, nullptr, &defaultSamplerNearest);
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
        vkCreateSampler(context.device, &samplerInfo, nullptr, &defaultSamplerLinear);
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
        vkCreateSampler(context.device, &samplerInfo, nullptr, &defaultSamplerMipMappedLinear);
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
        sceneDataLayout = layoutBuilder.build(
            context.device,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, nullptr,
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
        addressesLayout = layoutBuilder.build(context.device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }

    // Render Object Textures
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_SAMPLER, render_object_constants::MAX_SAMPLER_COUNT);
        layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, render_object_constants::MAX_IMAGES_COUNT);
        texturesLayout = layoutBuilder.build(context.device, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }
    // Render Targets
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // Normals
        layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // Albedo
        layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // PBR
        layoutBuilder.addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // Depth
        layoutBuilder.addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // Velocity
        layoutBuilder.addBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // AO
        layoutBuilder.addBinding(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // Screen Space Contact Shadows
        layoutBuilder.addBinding(7, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // Output

        renderTargetsLayout = layoutBuilder.build(context.device, VK_SHADER_STAGE_COMPUTE_BIT, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }

    // Terrain Textures
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, terrain::MAX_TERRAIN_TEXTURE_COUNT);

        terrainTexturesLayout = layoutBuilder.build(context.device, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }
    // Terrain Uniform
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

        terrainUniformLayout = layoutBuilder.build(context.device, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }
}

will_engine::ResourceManager::~ResourceManager()
{
    if (context.device == VK_NULL_HANDLE) { return; }

    destroyImage(whiteImage);
    destroyImage(errorCheckerboardImage);
    vkDestroySampler(context.device, defaultSamplerNearest, nullptr);
    vkDestroySampler(context.device, defaultSamplerLinear, nullptr);
    vkDestroySampler(context.device, defaultSamplerMipMappedLinear, nullptr);
    vkDestroyDescriptorSetLayout(context.device, emptyDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(context.device, sceneDataLayout, nullptr);
    vkDestroyDescriptorSetLayout(context.device, frustumCullLayout, nullptr);
    vkDestroyDescriptorSetLayout(context.device, addressesLayout, nullptr);
    vkDestroyDescriptorSetLayout(context.device, texturesLayout, nullptr);
    vkDestroyDescriptorSetLayout(context.device, renderTargetsLayout, nullptr);
    vkDestroyDescriptorSetLayout(context.device, terrainTexturesLayout, nullptr);
    vkDestroyDescriptorSetLayout(context.device, terrainUniformLayout, nullptr);
}

AllocatedBuffer will_engine::ResourceManager::createBuffer(const size_t allocSize, const VkBufferUsageFlags usage, const VmaMemoryUsage memoryUsage) const
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

AllocatedBuffer will_engine::ResourceManager::createHostSequentialBuffer(const size_t allocSize) const
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

AllocatedBuffer will_engine::ResourceManager::createHostRandomBuffer(const size_t allocSize, const VkBufferUsageFlags additionalUsages) const
{
    const VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = allocSize,
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | additionalUsages,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    constexpr VmaAllocationCreateInfo allocInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
    };
    AllocatedBuffer newBuffer{};
    VK_CHECK(vmaCreateBuffer(context.allocator, &bufferInfo, &allocInfo,&newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

    return newBuffer;
}

AllocatedBuffer will_engine::ResourceManager::createDeviceBuffer(const size_t allocSize, const VkBufferUsageFlags additionalUsages) const
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

AllocatedBuffer will_engine::ResourceManager::createStagingBuffer(const size_t allocSize) const
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

AllocatedBuffer will_engine::ResourceManager::createReceivingBuffer(const size_t allocSize) const
{
    return createBuffer(allocSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU);
}

void will_engine::ResourceManager::copyBuffer(const AllocatedBuffer& src, const AllocatedBuffer& dst, const VkDeviceSize size) const
{
    immediate.submit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{};
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = size;

        vkCmdCopyBuffer(cmd, src.buffer, dst.buffer, 1, &vertexCopy);
    });
}

void will_engine::ResourceManager::copyBuffer(const AllocatedBuffer& src, const AllocatedBuffer& dst, const VkDeviceSize size, const VkDeviceSize offset) const
{
    immediate.submit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{};
        vertexCopy.dstOffset = offset;
        vertexCopy.srcOffset = offset;
        vertexCopy.size = size;

        vkCmdCopyBuffer(cmd, src.buffer, dst.buffer, 1, &vertexCopy);
    });
}


VkDeviceAddress will_engine::ResourceManager::getBufferAddress(const AllocatedBuffer& buffer) const
{
    VkBufferDeviceAddressInfo addressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR};
    addressInfo.buffer = buffer.buffer;
    const VkDeviceAddress srcPtr = vkGetBufferDeviceAddress(context.device, &addressInfo);
    return srcPtr;
}

void will_engine::ResourceManager::destroyBuffer(AllocatedBuffer& buffer) const
{
    if (buffer.buffer == VK_NULL_HANDLE) { return; }
    vmaDestroyBuffer(context.allocator, buffer.buffer, buffer.allocation);
    buffer.buffer = VK_NULL_HANDLE;
}

VkSampler will_engine::ResourceManager::createSampler(const VkSamplerCreateInfo& createInfo) const
{
    VkSampler newSampler;
    vkCreateSampler(context.device, &createInfo, nullptr, &newSampler);
    return newSampler;
}


AllocatedImage will_engine::ResourceManager::createImage(const VkImageCreateInfo& createInfo) const
{
    AllocatedImage newImage{};
    newImage.imageFormat = createInfo.format;
    newImage.imageExtent = createInfo.extent;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    VK_CHECK(vmaCreateImage(context.allocator, &createInfo, &allocInfo, &newImage.image, &newImage.allocation, nullptr));

    const VkImageAspectFlags aspectFlag = createInfo.format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    // build an image-view for the image
    VkImageViewCreateInfo view_info = vk_helpers::imageviewCreateInfo(createInfo.format, newImage.image, aspectFlag);
    view_info.subresourceRange.levelCount = createInfo.mipLevels;

    VK_CHECK(vkCreateImageView(context.device, &view_info, nullptr, &newImage.imageView));

    return newImage;
}

AllocatedImage will_engine::ResourceManager::createImage(const VkExtent3D size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped) const
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

AllocatedImage will_engine::ResourceManager::createImage(const void* data, const size_t dataSize, const VkExtent3D size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped) const
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
        }
        else {
            vk_helpers::transitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
        }
    });

    destroyBuffer(uploadbuffer);

    return newImage;
}

AllocatedImage will_engine::ResourceManager::createCubemap(const VkExtent3D size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped) const
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

void will_engine::ResourceManager::destroyImage(AllocatedImage& img) const
{
    vkDestroyImageView(context.device, img.imageView, nullptr);
    vmaDestroyImage(context.allocator, img.image, img.allocation);
    img.image = VK_NULL_HANDLE;
    img.imageView = VK_NULL_HANDLE;
    img.allocation = VK_NULL_HANDLE;
    img.imageExtent = {};
    img.imageFormat = {};
}

void will_engine::ResourceManager::destroySampler(VkSampler& sampler) const
{
    vkDestroySampler(context.device, sampler, nullptr);
    sampler = VK_NULL_HANDLE;
}

will_engine::DescriptorBufferSampler will_engine::ResourceManager::createDescriptorBufferSampler(VkDescriptorSetLayout layout, int32_t maxObjectCount) const
{
    return DescriptorBufferSampler(context, layout, maxObjectCount);
}

int32_t will_engine::ResourceManager::setupDescriptorBufferSampler(DescriptorBufferSampler& descriptorBuffer, const std::vector<DescriptorImageData>& imageBuffers, const int index) const
{
    return descriptorBuffer.setupData(context.device, imageBuffers, index);
}

will_engine::DescriptorBufferUniform will_engine::ResourceManager::createDescriptorBufferUniform(VkDescriptorSetLayout layout, int32_t maxObjectCount) const
{
    return DescriptorBufferUniform(context, layout, maxObjectCount);
}

int32_t will_engine::ResourceManager::setupDescriptorBufferUniform(DescriptorBufferUniform& descriptorBuffer, const std::vector<DescriptorUniformData>& uniformBuffers, const int index) const
{
    return descriptorBuffer.setupData(context.device, uniformBuffers, index);
}

void will_engine::ResourceManager::destroyDescriptorBuffer(DescriptorBuffer& descriptorBuffer) const
{
    descriptorBuffer.destroy(context.allocator);
}

VkShaderModule will_engine::ResourceManager::createShaderModule(const std::filesystem::path& path) const
{
    auto start = std::chrono::system_clock::now();
    std::filesystem::path projectRoot = std::filesystem::current_path();
    // Pre-Compiled
    if (path.extension() == ".spv") {
        const std::filesystem::path shaderPath((projectRoot / path).string().c_str());
        std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error(fmt::format("Failed to read file {}", shaderPath.string()));
        }

        const size_t fileSize = file.tellg();
        std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

        file.close();

        // create a new shader module, using the buffer we loaded
        VkShaderModuleCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = buffer.size() * sizeof(uint32_t),
            .pCode = buffer.data(),
        };

        VkShaderModule shader;
        if (vkCreateShaderModule(context.device, &createInfo, nullptr, &shader) != VK_SUCCESS) {
            throw std::runtime_error(fmt::format("Failed to load shader {}", path.filename().string()));
        }

        const auto end = std::chrono::system_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        //fmt::print("Created shader module {} in {:.2f}ms\n", path.filename().string(), static_cast<float>(elapsed.count()) / 1000000.0f);
        return shader;
    }

    std::ifstream file(path.string());
    auto source = std::string(std::istreambuf_iterator(file), std::istreambuf_iterator<char>());
    shaderc_shader_kind kind;
    if (path.extension() == ".vert") kind = shaderc_vertex_shader;
    else if (path.extension() == ".frag") kind = shaderc_fragment_shader;
    else if (path.extension() == ".comp") kind = shaderc_compute_shader;
    else if (path.extension() == ".tesc") kind = shaderc_tess_control_shader;
    else if (path.extension() == ".tese") kind = shaderc_tess_evaluation_shader;

    else throw std::runtime_error(fmt::format("Unknown shader type: {}", path.extension().string()));

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    std::vector<std::string> include_paths = {"shaders/include"};
    options.SetIncluder(std::make_unique<CustomIncluder>(include_paths));
    auto result = compiler.CompileGlslToSpv(source, kind, "shader", options);

    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        fmt::print("Shader source:\n{}\n", source);
        fmt::print("Compilation error:\n{}\n", result.GetErrorMessage());
        throw std::runtime_error(result.GetErrorMessage());
    }

    std::vector<uint32_t> spirv = {result.cbegin(), result.cend()};
    VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spirv.size() * sizeof(uint32_t),
        .pCode = spirv.data()
    };

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(context.device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }

    const auto end = std::chrono::system_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    //fmt::print("Created shader module {} in {:.2f}ms\n", path.filename().string(), static_cast<float>(elapsed.count()) / 1000000.0f);
    return shaderModule;
}

void will_engine::ResourceManager::destroyShaderModule(VkShaderModule& shaderModule) const
{
    vkDestroyShaderModule(context.device, shaderModule, nullptr);
    shaderModule = VK_NULL_HANDLE;
}

VkPipelineLayout will_engine::ResourceManager::createPipelineLayout(const VkPipelineLayoutCreateInfo& createInfo) const
{
    VkPipelineLayout pipelineLayout;
    VK_CHECK(vkCreatePipelineLayout(context.device, &createInfo, nullptr, &pipelineLayout));
    return pipelineLayout;
}

void will_engine::ResourceManager::destroyPipelineLayout(VkPipelineLayout& pipelineLayout) const
{
    if (pipelineLayout == VK_NULL_HANDLE) { return; }
    vkDestroyPipelineLayout(context.device, pipelineLayout, nullptr);
    pipelineLayout = VK_NULL_HANDLE;
}

VkPipeline will_engine::ResourceManager::createRenderPipeline(PipelineBuilder& builder, const std::vector<VkDynamicState>& additionalDynamicStates) const
{
    const VkPipeline pipeline = builder.buildPipeline(context.device, VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT, additionalDynamicStates);
    return pipeline;
}

VkPipeline will_engine::ResourceManager::createComputePipeline(const VkComputePipelineCreateInfo& pipelineInfo) const
{
    VkPipeline computePipeline;
    vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline);
    return computePipeline;
}

void will_engine::ResourceManager::destroyPipeline(VkPipeline& pipeline) const
{
    if (pipeline == VK_NULL_HANDLE) { return; }
    vkDestroyPipeline(context.device, pipeline, nullptr);
    pipeline = VK_NULL_HANDLE;
}

VkDescriptorSetLayout will_engine::ResourceManager::createDescriptorSetLayout(DescriptorLayoutBuilder& layoutBuilder, const VkShaderStageFlagBits shaderStageFlags,
                                                                 const VkDescriptorSetLayoutCreateFlagBits layoutCreateFlags) const
{
    return layoutBuilder.build(context.device, shaderStageFlags, nullptr, layoutCreateFlags);
}

void will_engine::ResourceManager::destroyDescriptorSetLayout(VkDescriptorSetLayout& descriptorSetLayout) const
{
    if (descriptorSetLayout == VK_NULL_HANDLE) { return; }
    vkDestroyDescriptorSetLayout(context.device, descriptorSetLayout, nullptr);
    descriptorSetLayout = VK_NULL_HANDLE;
}

VkImageView will_engine::ResourceManager::createImageView(const VkImageViewCreateInfo& viewInfo) const
{
    VkImageView imageView;
    VK_CHECK(vkCreateImageView(context.device, &viewInfo, nullptr, &imageView));
    return imageView;
}

void will_engine::ResourceManager::destroyImageView(VkImageView& imageView) const
{
    if (imageView == VK_NULL_HANDLE) { return; }
    vkDestroyImageView(context.device, imageView, nullptr);
    imageView = VK_NULL_HANDLE;
}
