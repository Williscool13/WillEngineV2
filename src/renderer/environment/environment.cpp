//
// Created by William on 2024-08-25.
//

#include "environment.h"

#include <cassert>
#include <stb_image.h>

#include "src/renderer/immediate_submitter.h"
#include "src/renderer/resource_manager.h"
#include "src/renderer/vk_descriptor_buffer.h"
#include "src/renderer/pipelines/environment/environment_descriptor_layouts.h"
#include "src/util/file_utils.h"

const VkExtent3D Environment::SPECULAR_PREFILTERED_BASE_EXTENTS = {512, 512, 1};
const VkExtent3D Environment::LUT_IMAGE_EXTENT = {512, 512, 1};

Environment::Environment(VulkanContext& context, ResourceManager& resourceManager, ImmediateSubmitter& immediate, EnvironmentDescriptorLayouts& environmentDescriptorLayouts)
    : context(context), resourceManager(resourceManager), immediate(immediate), descriptorLayouts(environmentDescriptorLayouts)
{
    // sampler
    {
        VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

        sampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampl.minLod = 0;
        sampl.maxLod = VK_LOD_CLAMP_NONE;
        sampl.magFilter = VK_FILTER_LINEAR;
        sampl.minFilter = VK_FILTER_LINEAR;

        sampl.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampl.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampl.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        vkCreateSampler(context.device, &sampl, nullptr, &sampler);
    }


    // equi to cubemap pipeline
    {
        VkPushConstantRange pushConstantRange = {};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(EquiToCubePushConstantData);

        VkDescriptorSetLayout layouts[]{descriptorLayouts.getEquiImageLayout(), descriptorLayouts.getCubemapStorageLayout()};

        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 2;
        layout_info.pSetLayouts = layouts;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &pushConstantRange;

        VK_CHECK(vkCreatePipelineLayout(context.device, &layout_info, nullptr, &equiToCubemapPipelineLayout));


        VkShaderModule computeShader;
        if (!vk_helpers::loadShaderModule("shaders/environment/equitoface.comp.spv", context.device, &computeShader)) {
            fmt::print("Error when building the compute shader (equitoface.comp.spv)\n");
            abort();
        }

        VkPipelineShaderStageCreateInfo stageinfo{};
        stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageinfo.pNext = nullptr;
        stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageinfo.module = computeShader;
        stageinfo.pName = "main"; // entry point in shader

        VkComputePipelineCreateInfo computePipelineCreateInfo{};
        computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineCreateInfo.pNext = nullptr;
        computePipelineCreateInfo.layout = equiToCubemapPipelineLayout;
        computePipelineCreateInfo.stage = stageinfo;
        computePipelineCreateInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

        VK_CHECK(vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &equiToCubemapPipeline));

        vkDestroyShaderModule(context.device, computeShader, nullptr);
    }

    // cubemap to diffuse pipeline
    {
        VkDescriptorSetLayout layouts[]{descriptorLayouts.getCubemapSamplerLayout(), descriptorLayouts.getCubemapStorageLayout()};

        VkPushConstantRange pushConstantRange = {};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(CubeToDiffusePushConstantData);

        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 2;
        layout_info.pSetLayouts = layouts;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &pushConstantRange;

        VK_CHECK(vkCreatePipelineLayout(context.device, &layout_info, nullptr, &cubemapToDiffusePipelineLayout));

        VkShaderModule computeShader;
        if (!vk_helpers::loadShaderModule("shaders/environment/cubetodiffirra.comp.spv", context.device, &computeShader)) {
            fmt::print("Error when building the compute shader (cubetodiffspec.comp.spv)\n");
            abort();
        }

        VkPipelineShaderStageCreateInfo stageinfo{};
        stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageinfo.pNext = nullptr;
        stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageinfo.module = computeShader;
        stageinfo.pName = "main"; // entry point in shader

        VkComputePipelineCreateInfo computePipelineCreateInfo{};
        computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineCreateInfo.pNext = nullptr;
        computePipelineCreateInfo.layout = cubemapToDiffusePipelineLayout;
        computePipelineCreateInfo.stage = stageinfo;
        computePipelineCreateInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

        VK_CHECK(vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &cubemapToDiffusePipeline));

        vkDestroyShaderModule(context.device, computeShader, nullptr);
    }

    // cubemap to specular pipeline
    {
        VkDescriptorSetLayout layouts[]{descriptorLayouts.getCubemapSamplerLayout(), descriptorLayouts.getCubemapStorageLayout()};

        VkPushConstantRange pushConstantRange = {};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(CubeToPrefilteredConstantData);

        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 2;
        layout_info.pSetLayouts = layouts;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &pushConstantRange;

        VK_CHECK(vkCreatePipelineLayout(context.device, &layout_info, nullptr, &cubemapToSpecularPipelineLayout));

        VkShaderModule computeShader;
        if (!vk_helpers::loadShaderModule("shaders/environment/cubetospecprefilter.comp.spv", context.device, &computeShader)) {
            fmt::print("Error when building the compute shader (cubetospecprefilter.comp.spv)\n");
            abort();
        }

        VkPipelineShaderStageCreateInfo stageinfo{};
        stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageinfo.pNext = nullptr;
        stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageinfo.module = computeShader;
        stageinfo.pName = "main"; // entry point in shader

        VkComputePipelineCreateInfo computePipelineCreateInfo{};
        computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineCreateInfo.pNext = nullptr;
        computePipelineCreateInfo.layout = cubemapToSpecularPipelineLayout;
        computePipelineCreateInfo.stage = stageinfo;
        computePipelineCreateInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

        VK_CHECK(vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &cubemapToSpecularPipeline));

        vkDestroyShaderModule(context.device, computeShader, nullptr);
    }

    // lut generation pipeline
    {
        VkDescriptorSetLayout layouts[1]{descriptorLayouts.getLutLayout()};

        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = layouts;
        layout_info.pushConstantRangeCount = 0;
        layout_info.pPushConstantRanges = nullptr;

        VK_CHECK(vkCreatePipelineLayout(context.device, &layout_info, nullptr, &lutPipelineLayout));

        VkShaderModule computeShader;
        if (!vk_helpers::loadShaderModule("shaders/environment/brdflut.comp.spv", context.device, &computeShader)) {
            fmt::print("Error when building the compute shader (brdflut.comp.spv)\n");
            abort();
        }

        VkPipelineShaderStageCreateInfo stageinfo{};
        stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageinfo.pNext = nullptr;
        stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageinfo.module = computeShader;
        stageinfo.pName = "main"; // entry point in shader

        VkComputePipelineCreateInfo computePipelineCreateInfo{};
        computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineCreateInfo.pNext = nullptr;
        computePipelineCreateInfo.layout = lutPipelineLayout;
        computePipelineCreateInfo.stage = stageinfo;
        computePipelineCreateInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

        VK_CHECK(vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &lutPipeline));

        vkDestroyShaderModule(context.device, computeShader, nullptr);
    }

    // Create LUT here, because its the same for all environment maps
    {
        lutDescriptorBuffer = DescriptorBufferSampler(context, descriptorLayouts.getLutLayout(), 1);

        lutImage = resourceManager.createImage(LUT_IMAGE_EXTENT, VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

        VkDescriptorImageInfo lutDescriptorInfo{};
        lutDescriptorInfo.sampler = nullptr; // not sampled (storage)
        lutDescriptorInfo.imageView = lutImage.imageView;
        lutDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        std::vector<DescriptorImageData> descriptor = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, lutDescriptorInfo}};


        lutDescriptorBuffer.setupData(context.device, descriptor); // index 0, obviously
        generateLutImmediate();
    }

    equiImageDescriptorBuffer = DescriptorBufferSampler(context, descriptorLayouts.getEquiImageLayout(), 1);
    // 0 is original cubemap, 1 is diff irr, 2 is spec pref, 3 to 13 is for 10 mip levels of spec pref
    cubemapStorageDescriptorBuffer = DescriptorBufferSampler(context, descriptorLayouts.getCubemapStorageLayout(), 12);

    // sample cubemap
    cubemapDescriptorBuffer = DescriptorBufferSampler(context, descriptorLayouts.getCubemapSamplerLayout(), MAX_ENVIRONMENT_MAPS);
    diffSpecMapDescriptorBuffer = DescriptorBufferSampler(context, descriptorLayouts.getEnvironmentMapLayout(), MAX_ENVIRONMENT_MAPS);
}

Environment::~Environment()
{
    vkDestroyPipelineLayout(context.device, equiToCubemapPipelineLayout, nullptr);
    vkDestroyPipeline(context.device, equiToCubemapPipeline, nullptr);
    vkDestroyPipelineLayout(context.device, cubemapToDiffusePipelineLayout, nullptr);
    vkDestroyPipeline(context.device, cubemapToDiffusePipeline, nullptr);
    vkDestroyPipelineLayout(context.device, cubemapToSpecularPipelineLayout, nullptr);
    vkDestroyPipeline(context.device, cubemapToSpecularPipeline, nullptr);
    vkDestroyPipelineLayout(context.device, lutPipelineLayout, nullptr);
    vkDestroyPipeline(context.device, lutPipeline, nullptr);
    equiToCubemapPipelineLayout = VK_NULL_HANDLE;
    equiToCubemapPipeline = VK_NULL_HANDLE;
    cubemapToDiffusePipelineLayout = VK_NULL_HANDLE;
    cubemapToDiffusePipeline = VK_NULL_HANDLE;
    cubemapToSpecularPipelineLayout = VK_NULL_HANDLE;
    cubemapToSpecularPipeline = VK_NULL_HANDLE;

    lutDescriptorBuffer.destroy(context.allocator);
    resourceManager.destroyImage(lutImage);
    lutImage = {};

    vkDestroySampler(context.device, sampler, nullptr);

    cubemapStorageDescriptorBuffer.destroy(context.allocator);
    cubemapDescriptorBuffer.destroy(context.allocator);
    equiImageDescriptorBuffer.destroy(context.allocator);
    diffSpecMapDescriptorBuffer.destroy(context.allocator);

    for (EnvironmentMapData& envMap : environmentMaps) {
        resourceManager.destroyImage(envMap.cubemapImage);
        resourceManager.destroyImage(envMap.specDiffCubemap);
    }
}

void Environment::loadEnvironment(const char* name, const char* path, int environmentMapIndex)
{
    auto start = std::chrono::system_clock::now();
    std::filesystem::path cubemapPath{path};

    EnvironmentMapData newEnvMapData{};
    newEnvMapData.sourcePath = cubemapPath.string();
    VkExtent3D extents = {CUBEMAP_RESOLUTION, CUBEMAP_RESOLUTION, 1};


    assert(environmentMapIndex < MAX_ENVIRONMENT_MAPS && environmentMapIndex >= 0);
    AllocatedImage equiImage;
    int width, height, channels;
    if (float* imageData = stbi_loadf(newEnvMapData.sourcePath.c_str(), &width, &height, &channels, 4)) {
        equiImage = resourceManager.createImage(imageData, width * height * 4 * sizeof(float), VkExtent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
                                                VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, true);
        stbi_image_free(imageData);
    } else {
        fmt::print("Failed to load Equirectangular Image ({})\n", path);
    }
    assert(equiImage.imageExtent.width % 4 == 0);\
    assert(equiImage.imageExtent.width / 4 == CUBEMAP_RESOLUTION);

    // Equi -> Cubemap - recreate in case resolution changed
    newEnvMapData.cubemapImage = resourceManager.createCubemap(extents, VK_FORMAT_R32G32B32A32_SFLOAT,
                                                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                                               VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);

    // add new cubemap image to descriptor buffer
    VkDescriptorImageInfo equiImageDescriptorInfo{};
    equiImageDescriptorInfo.sampler = sampler;
    equiImageDescriptorInfo.imageView = equiImage.imageView;
    equiImageDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // needs to match the order of the bindings in the layout
    std::vector<DescriptorImageData> combined_descriptor = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, equiImageDescriptorInfo}};
    equiImageDescriptorBuffer.setupData(context.device, combined_descriptor);


    VkDescriptorImageInfo cubemapDescriptor{};
    cubemapDescriptor.sampler = sampler;
    cubemapDescriptor.imageView = newEnvMapData.cubemapImage.imageView;
    cubemapDescriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    std::vector<DescriptorImageData> cubemapStorageDescriptor = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, cubemapDescriptor}};
    int cubemapIndex = cubemapStorageDescriptorBuffer.setupData(context.device, cubemapStorageDescriptor);
    cubemapDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    std::vector<DescriptorImageData> cubemapSamplerDescriptor = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, cubemapDescriptor}};
    cubemapDescriptorBuffer.setupData(context.device, cubemapSamplerDescriptor, environmentMapIndex);


    equiToCubemapImmediate(newEnvMapData.cubemapImage, cubemapIndex);

    // can safely destroy the cubemap image view in the storage buffer
    resourceManager.destroyImage(equiImage);
    cubemapStorageDescriptorBuffer.freeDescriptorBuffer(cubemapIndex);
    equiImageDescriptorBuffer.freeDescriptorBuffer(0); // always 0


    auto end0 = std::chrono::system_clock::now();
    auto elapsed0 = std::chrono::duration_cast<std::chrono::microseconds>(end0 - start);
    fmt::print("Environment Map: {} | Cubemap {:.1f}ms | ", file_utils::getFileName(path), elapsed0.count() / 1000.0f);
    newEnvMapData.specDiffCubemap = resourceManager.createCubemap(SPECULAR_PREFILTERED_BASE_EXTENTS, VK_FORMAT_R32G32B32A32_SFLOAT,
                                                                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                                                  VK_IMAGE_USAGE_STORAGE_BIT, true);


    AllocatedCubemap specDiffCubemap = {};
    specDiffCubemap.allocatedImage = newEnvMapData.specDiffCubemap;
    specDiffCubemap.mipLevels = SPECULAR_PREFILTERED_MIP_LEVELS;
    specDiffCubemap.cubemapImageViews = std::vector<CubemapImageView>(SPECULAR_PREFILTERED_MIP_LEVELS);
    assert(SPECULAR_PREFILTERED_BASE_EXTENTS.width == SPECULAR_PREFILTERED_BASE_EXTENTS.height);

    for (int i = 0; i < SPECULAR_PREFILTERED_MIP_LEVELS; i++) {
        CubemapImageView cubemapImageView{};
        VkImageViewCreateInfo view_info = vk_helpers::cubemapViewCreateInfo(newEnvMapData.specDiffCubemap.imageFormat, newEnvMapData.specDiffCubemap.image, VK_IMAGE_ASPECT_COLOR_BIT);
        view_info.subresourceRange.baseMipLevel = i;
        VK_CHECK(vkCreateImageView(context.device, &view_info, nullptr, &cubemapImageView.imageView));

        auto length = static_cast<uint32_t>(SPECULAR_PREFILTERED_BASE_EXTENTS.width / pow(2, i)); // w and h always equal
        cubemapImageView.imageExtent = {length, length, 1};
        float roughness;
        int j = i;
        if (i == DIFFUSE_IRRADIANCE_MIP_LEVEL) {
            roughness = -1;
        } // diffuse irradiance map
        else {
            roughness = static_cast<float>(j) / static_cast<float>(SPECULAR_PREFILTERED_MIP_LEVELS - 2);
        }

        cubemapImageView.roughness = roughness;

        VkDescriptorImageInfo prefilteredCubemapStorage{};
        prefilteredCubemapStorage.sampler = nullptr; // sampler not actually used in storage image
        prefilteredCubemapStorage.imageView = cubemapImageView.imageView;
        prefilteredCubemapStorage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        std::vector<DescriptorImageData> prefilteredCubemapStorageDescriptor = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, prefilteredCubemapStorage}};

        int descriptorBufferIndex = cubemapStorageDescriptorBuffer.setupData(context.device, prefilteredCubemapStorageDescriptor);
        cubemapImageView.descriptorBufferIndex = descriptorBufferIndex;

        specDiffCubemap.cubemapImageViews[i] = cubemapImageView;
    }

    cubemapToDiffuseSpecularImmediate(specDiffCubemap, environmentMapIndex);
    // can safely destroy all the mip level image views
    for (int i = 0; i < specDiffCubemap.mipLevels; i++) {
        vkDestroyImageView(context.device, specDiffCubemap.cubemapImageViews[i].imageView, nullptr);
        cubemapStorageDescriptorBuffer.freeDescriptorBuffer(specDiffCubemap.cubemapImageViews[i].descriptorBufferIndex);
    }


    auto end1 = std::chrono::system_clock::now();
    auto elapsed1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - end0);
    fmt::print("D/S Map {:.1f}ms | ", elapsed1.count() / 1000.0f);


    VkDescriptorImageInfo diffSpecDescriptorInfo{};
    diffSpecDescriptorInfo.sampler = sampler;
    diffSpecDescriptorInfo.imageView = newEnvMapData.specDiffCubemap.imageView;
    diffSpecDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo lutDescriptorInfo{};
    lutDescriptorInfo.sampler = sampler;
    lutDescriptorInfo.imageView = lutImage.imageView;
    lutDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    std::vector<DescriptorImageData> combinedDescriptor2 = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, diffSpecDescriptorInfo},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, lutDescriptorInfo}
    };

    diffSpecMapDescriptorBuffer.setupData(context.device, combinedDescriptor2, environmentMapIndex);

    environmentMaps[environmentMapIndex] = newEnvMapData;

    activeEnvironmentMapNames.insert({environmentMapIndex, name});

    auto end3 = std::chrono::system_clock::now();
    auto elapsed3 = std::chrono::duration_cast<std::chrono::microseconds>(end3 - start);

    fmt::print("Total {:.1f}ms\n", elapsed3.count() / 1000.0f);
}

void Environment::equiToCubemapImmediate(const AllocatedImage& _cubemapImage, const int _cubemapStorageDescriptorIndex) const
{
    immediate.submit([&](VkCommandBuffer cmd) {
        constexpr VkExtent3D extents = {CUBEMAP_RESOLUTION, CUBEMAP_RESOLUTION, 1};

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, equiToCubemapPipeline);

        vk_helpers::transitionImage(cmd, _cubemapImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

        VkDescriptorBufferBindingInfoEXT descriptor_buffer_binding_info[2];

        descriptor_buffer_binding_info[0] = equiImageDescriptorBuffer.getDescriptorBufferBindingInfo();
        descriptor_buffer_binding_info[1] = cubemapStorageDescriptorBuffer.getDescriptorBufferBindingInfo();

        vkCmdBindDescriptorBuffersEXT(cmd, 2, descriptor_buffer_binding_info);
        constexpr uint32_t equiImageIndex = 0;
        constexpr uint32_t cubemapIndex = 1;

        constexpr VkDeviceSize offset = 0;
        const VkDeviceSize cubemapOffset = _cubemapStorageDescriptorIndex * cubemapStorageDescriptorBuffer.getDescriptorBufferSize();
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, equiToCubemapPipelineLayout
                                           , 0, 1, &equiImageIndex, &offset);
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, equiToCubemapPipelineLayout
                                           , 1, 1, &cubemapIndex, &cubemapOffset);


        EquiToCubePushConstantData pushData{};
        pushData.flipY = FLIP_Y;
        vkCmdPushConstants(cmd, equiToCubemapPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(EquiToCubePushConstantData), &pushData);

        assert(extents.width % 16 == 0 && extents.height % 16 == 0);
        vkCmdDispatch(cmd, extents.width / 16, extents.height / 16, 6);

        vk_helpers::transitionImage(cmd, _cubemapImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    });
}

void Environment::cubemapToDiffuseSpecularImmediate(const AllocatedCubemap& cubemapMips, const int cubemapSampleDescriptorIndex) const
{
    immediate.submit([&](VkCommandBuffer cmd) {
        vk_helpers::transitionImage(cmd, cubemapMips.allocatedImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);


        VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[2];
        descriptorBufferBindingInfo[0] = cubemapDescriptorBuffer.getDescriptorBufferBindingInfo();
        descriptorBufferBindingInfo[1] = cubemapStorageDescriptorBuffer.getDescriptorBufferBindingInfo();

        constexpr uint32_t cubemapIndex = 0;
        constexpr uint32_t storageCubemapIndex = 1;
        const VkDeviceSize sampleOffset = cubemapSampleDescriptorIndex * cubemapDescriptorBuffer.getDescriptorBufferSize();
        // Diffuse
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapToDiffusePipeline);
            vkCmdBindDescriptorBuffersEXT(cmd, 2, descriptorBufferBindingInfo);
            vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapToDiffusePipelineLayout, 0, 1, &cubemapIndex, &sampleOffset);

            const CubemapImageView& diffuse = cubemapMips.cubemapImageViews[5];
            assert(diffuse.roughness == -1);

            const VkDeviceSize diffusemapOffset = cubemapStorageDescriptorBuffer.getDescriptorBufferSize() * diffuse.descriptorBufferIndex;
            vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapToDiffusePipelineLayout, 1, 1, &storageCubemapIndex, &diffusemapOffset);

            CubeToDiffusePushConstantData pushData{};
            pushData.sampleDelta = DIFFUFE_SAMPLE_DELTA;
            vkCmdPushConstants(cmd, cubemapToDiffusePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CubeToDiffusePushConstantData), &pushData);

            // width and height should be 32, dont need bounds check in shader code
            const auto xDispatch = static_cast<uint32_t>(std::ceil(diffuse.imageExtent.width / 8.0f));
            const auto yDispatch = static_cast<uint32_t>(std::ceil(diffuse.imageExtent.height / 8.0f));

            vkCmdDispatch(cmd, xDispatch, yDispatch, 6);
        }


        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapToSpecularPipeline);
        vkCmdBindDescriptorBuffersEXT(cmd, 2, descriptorBufferBindingInfo);
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapToSpecularPipelineLayout, 0, 1, &cubemapIndex,
                                           &sampleOffset);

        for (int i = 0; i < SPECULAR_PREFILTERED_MIP_LEVELS; i++) {
            if (i == 5) continue; // skip the diffuse map mip level
            const CubemapImageView& current = cubemapMips.cubemapImageViews[i];

            VkDeviceSize irradiancemap_offset = cubemapStorageDescriptorBuffer.getDescriptorBufferSize() * current.descriptorBufferIndex;
            vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapToSpecularPipelineLayout, 1, 1, &storageCubemapIndex,
                                               &irradiancemap_offset);

            CubeToPrefilteredConstantData pushData{};
            pushData.roughness = current.roughness;
            pushData.imageWidth = current.imageExtent.width;
            pushData.imageHeight = current.imageExtent.height;
            pushData.sampleCount = static_cast<uint32_t>(SPECULAR_SAMPLE_COUNT);
            vkCmdPushConstants(cmd, cubemapToSpecularPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CubeToDiffusePushConstantData),
                               &pushData);

            const auto xDispatch = static_cast<uint32_t>(std::ceil(current.imageExtent.width / 8.0f));
            const auto yDispatch = static_cast<uint32_t>(std::ceil(current.imageExtent.height / 8.0f));


            vkCmdDispatch(cmd, xDispatch, yDispatch, 6);
        }


        vk_helpers::transitionImage(cmd, cubemapMips.allocatedImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    });
}

void Environment::generateLutImmediate() const
{
    immediate.submit([&](VkCommandBuffer cmd) {
        vk_helpers::transitionImage(cmd, lutImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, lutPipeline);

        VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[1];
        descriptorBufferBindingInfo[0] = lutDescriptorBuffer.getDescriptorBufferBindingInfo();

        constexpr uint32_t lutIndex = 0;
        constexpr VkDeviceSize zeroOffset = 0;

        vkCmdBindDescriptorBuffersEXT(cmd, 1, descriptorBufferBindingInfo);
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, lutPipelineLayout, 0, 1, &lutIndex, &zeroOffset);

        // must be divisible by 16, dont want to bother bounds checking in shader code
        assert(LUT_IMAGE_EXTENT.width % 8 == 0 && LUT_IMAGE_EXTENT.height % 8 == 0);
        const uint32_t xDisp = LUT_IMAGE_EXTENT.width / 8;
        const uint32_t yDisp = LUT_IMAGE_EXTENT.height / 8;
        constexpr uint32_t z_disp = 1;
        vkCmdDispatch(cmd, xDisp, yDisp, z_disp);

        vk_helpers::transitionImage(cmd, lutImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    });
}
