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
#include "resources/descriptor_set_layout.h"
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
        defaultSamplerNearest = createSampler(samplerInfo);
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
        defaultSamplerLinear = createSampler(samplerInfo);
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
        defaultSamplerMipMappedLinear = createSampler(samplerInfo);
    }

    // Empty (WIP/unused)
    {
        DescriptorLayoutBuilder layoutBuilder;
        emptyDescriptorSetLayout = createDescriptorSetLayout(
            layoutBuilder,
            static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT),
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );
    }

    // Scene Data Layout
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        sceneDataLayout = createDescriptorSetLayout(
            layoutBuilder,
            static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT |
                                               VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );
    }
    // Frustum Cull Layout
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        frustumCullLayout = createDescriptorSetLayout(
            layoutBuilder,
            VK_SHADER_STAGE_COMPUTE_BIT,
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );
    }

    // Render Object Addresses
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        addressesLayout = createDescriptorSetLayout(
            layoutBuilder,
            static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT),
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );
    }

    // Render Object Textures
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_SAMPLER, render_object_constants::MAX_SAMPLER_COUNT);
        layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, render_object_constants::MAX_IMAGES_COUNT);
        texturesLayout = createDescriptorSetLayout(
            layoutBuilder,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );
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

        renderTargetsLayout = createDescriptorSetLayout(
            layoutBuilder,
            VK_SHADER_STAGE_COMPUTE_BIT,
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );
    }

    // Terrain Textures
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, terrain::MAX_TERRAIN_TEXTURE_COUNT);

        terrainTexturesLayout = createDescriptorSetLayout(
            layoutBuilder,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );
    }
    // Terrain Uniform
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

        terrainUniformLayout = createDescriptorSetLayout(
            layoutBuilder,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );
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

    destructionQueues[currentFrameOverlap].flush(context);
    lastKnownFrameOverlap = currentFrameOverlap;
}

void ResourceManager::flushDestructionQueue()
{
    for (int32_t i = 0; i < FRAME_OVERLAP; ++i) {
        destructionQueues[i].flush(context);
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

void ResourceManager::copyBufferImmediate(const AllocatedBuffer& src, const AllocatedBuffer& dst,
                                          const VkDeviceSize size) const
{
    copyBufferImmediate(src, dst, size, 0);
}

void ResourceManager::copyBufferImmediate(const AllocatedBuffer& src, const AllocatedBuffer& dst, const VkDeviceSize size,
                                          const VkDeviceSize offset) const
{
    immediate.submit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{};
        vertexCopy.dstOffset = offset;
        vertexCopy.srcOffset = offset;
        vertexCopy.size = size;

        vkCmdCopyBuffer(cmd, src.buffer, dst.buffer, 1, &vertexCopy);
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


VkDeviceAddress ResourceManager::getBufferAddress(const AllocatedBuffer& buffer) const
{
    VkBufferDeviceAddressInfo addressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR};
    addressInfo.buffer = buffer.buffer;
    const VkDeviceAddress srcPtr = vkGetBufferDeviceAddress(context.device, &addressInfo);
    return srcPtr;
}

AllocatedImage ResourceManager::createImageFromData(const void* data, size_t dataSize, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                                                    bool mipmapped)
{
    const size_t data_size = dataSize;
    AllocatedBuffer uploadBuffer = createStagingBuffer(data_size);
    memcpy(uploadBuffer.info.pMappedData, data, data_size);

    AllocatedImage newImage = createImage(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

    immediate.submit([&](VkCommandBuffer cmd) {
        vk_helpers::imageBarrier(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

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
        vkCmdCopyBufferToImage(cmd, uploadBuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        if (mipmapped) {
            vk_helpers::generateMipmaps(cmd, newImage.image, VkExtent2D{newImage.imageExtent.width, newImage.imageExtent.height});
        }
        else {
            vk_helpers::imageBarrier(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                     VK_IMAGE_ASPECT_COLOR_BIT);
        }
    });

    destroyResourceImmediate(std::move(uploadBuffer));

    return newImage;
}

DescriptorBufferSampler ResourceManager::createDescriptorBufferSampler(VkDescriptorSetLayout layout, int32_t maxObjectCount) const
{
    return DescriptorBufferSampler::create(context, layout, maxObjectCount);
}

int32_t ResourceManager::setupDescriptorBufferSampler(DescriptorBufferSampler& descriptorBuffer, const std::vector<DescriptorImageData>& imageBuffers,
    int index) const
{
    return descriptorBuffer.setupData(context, imageBuffers, index);
}

DescriptorBufferUniform ResourceManager::createDescriptorBufferUniform(VkDescriptorSetLayout layout, int32_t maxObjectCount) const
{
    return DescriptorBufferUniform::create(context, layout, maxObjectCount);
}

int32_t ResourceManager::setupDescriptorBufferUniform(DescriptorBufferUniform& descriptorBuffer,
    const std::vector<DescriptorUniformData>& uniformBuffers, int index) const
{
    return descriptorBuffer.setupData(context, uniformBuffers, index);
}

VkShaderModule ResourceManager::createShaderModule(const std::filesystem::path& path) const
{
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

    // Macros
    if (NORMAL_REMAP) {
        options.AddMacroDefinition("REMAP_NORMALS");
    }

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

    return shaderModule;
}

void ResourceManager::destroyShaderModule(VkShaderModule& module) const
{
    vkDestroyShaderModule(context.device, module, nullptr);
    module = VK_NULL_HANDLE;
}

DescriptorSetLayout ResourceManager::createDescriptorSetLayout(DescriptorLayoutBuilder& layoutBuilder,
                                                               VkShaderStageFlagBits shaderStageFlags,
                                                               VkDescriptorSetLayoutCreateFlags layoutCreateFlags) const
{
    for (auto& b : layoutBuilder.bindings) {
        b.stageFlags |= shaderStageFlags;
    }

    const VkDescriptorSetLayoutCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = layoutCreateFlags,
        .bindingCount = static_cast<uint32_t>(layoutBuilder.bindings.size()),
        .pBindings = layoutBuilder.bindings.data(),
    };

    return DescriptorSetLayout::create(context.device, createInfo);
}
}
