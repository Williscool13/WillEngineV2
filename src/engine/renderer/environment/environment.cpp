//
// Created by William on 2025-01-25.
//

#include "environment.h"

#include <stb/stb_image.h>
#include <vulkan/vulkan_core.h>

#include "environment_constants.h"
#include "engine/renderer/immediate_submitter.h"
#include "engine/renderer/resource_manager.h"
#include "engine/renderer/vk_descriptors.h"
#include "engine/renderer/resources/image.h"
#include "engine/renderer/resources/image_view.h"
#include "engine/util/file.h"

namespace will_engine::renderer
{
Environment::Environment(ResourceManager& resourceManager, ImmediateSubmitter& immediate)
    : resourceManager(resourceManager), immediate(immediate)
{
    // Descriptor Set Layouts
    {
        DescriptorLayoutBuilder layoutBuilder{2};
        //  Equirectangular Image Descriptor Set Layout
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        VkDescriptorSetLayoutCreateInfo createInfo = layoutBuilder.build(
            static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT),
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

        equiImageLayout = resourceManager.createResource<DescriptorSetLayout>(createInfo);

        //  (Storage) Cubemap Descriptor Set Layout
        layoutBuilder.clear();
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        createInfo = layoutBuilder.build(
            static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT),
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
        cubemapStorageLayout = resourceManager.createResource<DescriptorSetLayout>(createInfo);

        //  (Sampler) Cubemap Descriptor Set Layout
        layoutBuilder.clear();
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        createInfo = layoutBuilder.build(
            static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
        cubemapSamplerLayout = resourceManager.createResource<DescriptorSetLayout>(createInfo);

        // LUT generation
        layoutBuilder.clear();
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        createInfo = layoutBuilder.build(
            VK_SHADER_STAGE_COMPUTE_BIT,
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
        lutLayout = resourceManager.createResource<DescriptorSetLayout>(createInfo);


        // Full Cubemap Descriptor - contains PBR IBL data
        layoutBuilder.clear();
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // 1 cubemap  - diffuse/spec
        layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // 1 2d image - lut
        createInfo = layoutBuilder.build(
            static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT),
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
        environmentIBLLayout = resourceManager.createResource<DescriptorSetLayout>(createInfo);
    }

    VkSamplerCreateInfo samplerInfo = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;

    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    sampler = resourceManager.createResource<Sampler>(samplerInfo);

    // Equi -> Cubemap Pipeline
    {
        std::array layouts{equiImageLayout->layout, cubemapStorageLayout->layout};

        VkPipelineLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutCreateInfo.setLayoutCount = 2;
        layoutCreateInfo.pSetLayouts = layouts.data();
        layoutCreateInfo.pushConstantRangeCount = 0;
        layoutCreateInfo.pPushConstantRanges = nullptr;

        equiToCubemapPipelineLayout = resourceManager.createResource<PipelineLayout>(layoutCreateInfo);

        VkShaderModule computeShader = resourceManager.createShaderModule("shaders/environment/equitoface.comp");

        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.pNext = nullptr;
        stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageInfo.module = computeShader;
        stageInfo.pName = "main";

        VkComputePipelineCreateInfo computePipelineCreateInfo{};
        computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineCreateInfo.pNext = nullptr;
        computePipelineCreateInfo.layout = equiToCubemapPipelineLayout->layout;
        computePipelineCreateInfo.stage = stageInfo;
        computePipelineCreateInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

        equiToCubemapPipeline = resourceManager.createResource<Pipeline>(computePipelineCreateInfo);
        resourceManager.destroyShaderModule(computeShader);
    }

    // Cubemap -> Diff Pipeline
    {
        std::array layouts{cubemapSamplerLayout->layout, cubemapStorageLayout->layout};

        VkPushConstantRange pushConstantRange = {};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(CubeToDiffusePushConstantData);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 2;
        layoutInfo.pSetLayouts = layouts.data();
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstantRange;

        cubemapToDiffusePipelineLayout = resourceManager.createResource<PipelineLayout>(layoutInfo);

        VkShaderModule computeShader = resourceManager.createShaderModule("shaders/environment/cubetodiffirra.comp");

        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.pNext = nullptr;
        stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageInfo.module = computeShader;
        stageInfo.pName = "main";

        VkComputePipelineCreateInfo computePipelineCreateInfo{};
        computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineCreateInfo.pNext = nullptr;
        computePipelineCreateInfo.layout = cubemapToDiffusePipelineLayout->layout;
        computePipelineCreateInfo.stage = stageInfo;
        computePipelineCreateInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

        cubemapToDiffusePipeline = resourceManager.createResource<Pipeline>(computePipelineCreateInfo);
        resourceManager.destroyShaderModule(computeShader);
    }

    // Cubemap -> Spec Pipeline
    {
        std::array layouts{cubemapSamplerLayout->layout, cubemapStorageLayout->layout};

        VkPushConstantRange pushConstantRange = {};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(CubeToPrefilteredConstantData);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 2;
        layoutInfo.pSetLayouts = layouts.data();
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstantRange;

        cubemapToSpecularPipelineLayout = resourceManager.createResource<PipelineLayout>(layoutInfo);

        VkShaderModule computeShader = resourceManager.createShaderModule("shaders/environment/cubetospecprefilter.comp");

        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.pNext = nullptr;
        stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageInfo.module = computeShader;
        stageInfo.pName = "main";

        VkComputePipelineCreateInfo computePipelineCreateInfo{};
        computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineCreateInfo.pNext = nullptr;
        computePipelineCreateInfo.layout = cubemapToSpecularPipelineLayout->layout;
        computePipelineCreateInfo.stage = stageInfo;
        computePipelineCreateInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

        cubemapToSpecularPipeline = resourceManager.createResource<Pipeline>(computePipelineCreateInfo);
        resourceManager.destroyShaderModule(computeShader);
    }

    // LUT Generation
    {
        std::array layouts{lutLayout->layout};

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = layouts.data();
        layoutInfo.pushConstantRangeCount = 0;
        layoutInfo.pPushConstantRanges = nullptr;

        lutPipelineLayout = resourceManager.createResource<PipelineLayout>(layoutInfo);

        VkShaderModule computeShader = resourceManager.createShaderModule("shaders/environment/brdflut.comp");

        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.pNext = nullptr;
        stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageInfo.module = computeShader;
        stageInfo.pName = "main";

        VkComputePipelineCreateInfo computePipelineCreateInfo{};
        computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineCreateInfo.pNext = nullptr;
        computePipelineCreateInfo.layout = lutPipelineLayout->layout;
        computePipelineCreateInfo.stage = stageInfo;
        computePipelineCreateInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

        lutPipeline = resourceManager.createResource<Pipeline>(computePipelineCreateInfo);
        resourceManager.destroyShaderModule(computeShader);
    }


    // Execute LUT Generation
    {
        lutDescriptorBuffer = resourceManager.createResource<DescriptorBufferSampler>(lutLayout->layout, 1);

        VkImageUsageFlags usage{};
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        VkImageCreateInfo imgInfo = vk_helpers::imageCreateInfo(VK_FORMAT_R32G32_SFLOAT, usage, LUT_IMAGE_EXTENT);
        lutImage = resourceManager.createResource<Image>(imgInfo);

        VkDescriptorImageInfo lutDescriptorInfo{};
        lutDescriptorInfo.sampler = nullptr; // not sampled (storage)
        lutDescriptorInfo.imageView = lutImage->imageView;
        lutDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        std::vector<DescriptorImageData> descriptor = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, lutDescriptorInfo}};

        lutDescriptorBuffer->setupData(descriptor, 0);

        immediate.submit([&](VkCommandBuffer cmd) {
            vk_helpers::imageBarrier(cmd, lutImage->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, lutPipeline->pipeline);

            VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[1];
            descriptorBufferBindingInfo[0] = lutDescriptorBuffer->getBindingInfo();

            constexpr uint32_t lutIndex = 0;
            constexpr VkDeviceSize zeroOffset = 0;

            vkCmdBindDescriptorBuffersEXT(cmd, 1, descriptorBufferBindingInfo);
            vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, lutPipelineLayout->layout, 0, 1, &lutIndex, &zeroOffset);

            // must be divisible by 16, dont want to bother bounds checking in shader code
            assert(LUT_IMAGE_EXTENT.width % 8 == 0 && LUT_IMAGE_EXTENT.height % 8 == 0);
            const uint32_t xDisp = LUT_IMAGE_EXTENT.width / 8;
            const uint32_t yDisp = LUT_IMAGE_EXTENT.height / 8;
            constexpr uint32_t z_disp = 1;
            vkCmdDispatch(cmd, xDisp, yDisp, z_disp);

            vk_helpers::imageBarrier(cmd, lutImage->image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                     VK_IMAGE_ASPECT_COLOR_BIT);
        });
    }

    equiImageDescriptorBuffer = resourceManager.createResource<DescriptorBufferSampler>(equiImageLayout->layout, 1);
    // 0 will be filled and cleared for equi->cubemap
    // 0 to 4 and 5 will be filled with spec + diffuse cubemaps but will be cleared right after.
    // This will not exceed 6 size
    cubemapStorageDescriptorBuffer = resourceManager.createResource<DescriptorBufferSampler>(cubemapStorageLayout->layout, 6);

    // sample cubemap
    cubemapDescriptorBuffer = resourceManager.createResource<DescriptorBufferSampler>(cubemapSamplerLayout->layout, MAX_ENVIRONMENT_MAPS);
    diffSpecMapDescriptorBuffer = resourceManager.createResource<DescriptorBufferSampler>(environmentIBLLayout->layout, MAX_ENVIRONMENT_MAPS);

    environmentMaps[0] = {"Blank Environment"};
    activeEnvironmentMapNames.insert({0, "Blank Environment"});
}

Environment::~Environment()
{
    resourceManager.destroyResource(std::move(equiToCubemapPipelineLayout));
    resourceManager.destroyResource(std::move(cubemapToDiffusePipelineLayout));
    resourceManager.destroyResource(std::move(cubemapToSpecularPipelineLayout));
    resourceManager.destroyResource(std::move(lutPipelineLayout));
    resourceManager.destroyResource(std::move(equiToCubemapPipeline));
    resourceManager.destroyResource(std::move(cubemapToDiffusePipeline));
    resourceManager.destroyResource(std::move(cubemapToSpecularPipeline));
    resourceManager.destroyResource(std::move(lutPipeline));


    resourceManager.destroyResource(std::move(sampler));

    resourceManager.destroyResource(std::move(cubemapStorageDescriptorBuffer));
    resourceManager.destroyResource(std::move(cubemapDescriptorBuffer));
    resourceManager.destroyResource(std::move(equiImageDescriptorBuffer));
    resourceManager.destroyResource(std::move(diffSpecMapDescriptorBuffer));
    resourceManager.destroyResource(std::move(lutDescriptorBuffer));

    resourceManager.destroyResource(std::move(lutImage));
    lutImage = {};
    for (EnvironmentMapEntry& envMap : environmentMaps) {
        resourceManager.destroyResource(std::move(envMap.cubemapImage));
        resourceManager.destroyResource(std::move(envMap.specDiffCubemap));
    }


    resourceManager.destroyResource(std::move(equiImageLayout));
    resourceManager.destroyResource(std::move(cubemapStorageLayout));
    resourceManager.destroyResource(std::move(cubemapSamplerLayout));
    resourceManager.destroyResource(std::move(lutLayout));
    resourceManager.destroyResource(std::move(environmentIBLLayout));
}

void Environment::loadEnvironment(const char* name, const char* path, int32_t environmentMapIndex)
{
    auto start = std::chrono::system_clock::now();
    std::filesystem::path cubemapPath{path};

    EnvironmentMapEntry newEnvironmentEntry{};
    newEnvironmentEntry.sourcePath = cubemapPath.string();

    environmentMapIndex += 1;
    if (environmentMapIndex >= MAX_ENVIRONMENT_MAPS || environmentMapIndex < 0) {
        environmentMapIndex = glm::clamp(environmentMapIndex, 0, MAX_ENVIRONMENT_MAPS - 1);
        fmt::print("Environment map index out of range ({}). Clamping to 0 - {}\n", environmentMapIndex, MAX_ENVIRONMENT_MAPS - 1);
    }

    ImageResourcePtr equiImage;
    int32_t width, height, channels;
    if (float* imageData = stbi_loadf(newEnvironmentEntry.sourcePath.c_str(), &width, &height, &channels, 4)) {
        equiImage = resourceManager.createImageFromData(imageData, width * height * 4 * sizeof(float),
                                                        VkExtent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
                                                        VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, true);
        stbi_image_free(imageData);
    }
    else {
        fmt::print("Failed to load Equirectangular Image ({})\n", path);
        return;
    }

    if (equiImage->imageExtent.width % 4 != 0 || equiImage->imageExtent.width / 4 != CUBEMAP_RESOLUTION) {
        fmt::print("Dimensions of the equirectangular image is incorrect. Failed to load environment map ({})", path);
        return;
    }


    // Equirectangular -> Cubemap - recreate in case resolution changed
    newEnvironmentEntry.cubemapImage = resourceManager.createCubemapImage(CUBEMAP_EXTENTS, VK_FORMAT_R32G32B32A32_SFLOAT,
                                                                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                                                          VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, true);

    // add new cubemap image to descriptor buffer
    VkDescriptorImageInfo equiImageDescriptorInfo{};
    equiImageDescriptorInfo.sampler = sampler->sampler;
    equiImageDescriptorInfo.imageView = equiImage->imageView;
    equiImageDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // needs to match the order of the bindings in the layout
    std::vector<DescriptorImageData> combined_descriptor = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, equiImageDescriptorInfo}};
    int32_t equipIndex = equiImageDescriptorBuffer->setupData(combined_descriptor);

    VkDescriptorImageInfo cubemapDescriptor{};
    cubemapDescriptor.sampler = sampler->sampler;
    cubemapDescriptor.imageView = newEnvironmentEntry.cubemapImage->imageView;
    cubemapDescriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    std::vector<DescriptorImageData> cubemapStorageDescriptor = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, cubemapDescriptor}};
    int32_t cubemapIndex = cubemapStorageDescriptorBuffer->setupData(cubemapStorageDescriptor);

    cubemapDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    std::vector<DescriptorImageData> cubemapSamplerDescriptor = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, cubemapDescriptor}};
    cubemapDescriptorBuffer->setupData(cubemapSamplerDescriptor, environmentMapIndex);


    immediate.submit([&](VkCommandBuffer cmd) {
        vk_helpers::imageBarrier(cmd, newEnvironmentEntry.cubemapImage.get(), VK_IMAGE_LAYOUT_GENERAL,
                                 VK_IMAGE_ASPECT_COLOR_BIT);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, equiToCubemapPipeline->pipeline);

        VkDescriptorBufferBindingInfoEXT descriptor_buffer_binding_info[2];
        descriptor_buffer_binding_info[0] = equiImageDescriptorBuffer->getBindingInfo();
        descriptor_buffer_binding_info[1] = cubemapStorageDescriptorBuffer->getBindingInfo();
        vkCmdBindDescriptorBuffersEXT(cmd, 2, descriptor_buffer_binding_info);

        constexpr uint32_t _equiImageIndex = 0;
        constexpr uint32_t _cubemapIndex = 1;
        const VkDeviceSize equiOffset = equipIndex * equiImageDescriptorBuffer->getDescriptorBufferSize();
        const VkDeviceSize cubemapOffset = cubemapIndex * cubemapStorageDescriptorBuffer->getDescriptorBufferSize();
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, equiToCubemapPipelineLayout->layout, 0, 1, &_equiImageIndex,
                                           &equiOffset);
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, equiToCubemapPipelineLayout->layout, 1, 1, &_cubemapIndex,
                                           &cubemapOffset);

        vkCmdDispatch(cmd, CUBEMAP_EXTENTS.width / 16, CUBEMAP_EXTENTS.height / 16, 6);

        vk_helpers::generateMipmapsCubemap(cmd, newEnvironmentEntry.cubemapImage->image, {CUBEMAP_EXTENTS.width, CUBEMAP_EXTENTS.height},
                                           VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    });


    // can safely destroy the cubemap image view in the storage buffer
    resourceManager.destroyResource(std::move(equiImage));
    cubemapStorageDescriptorBuffer->freeDescriptorBufferIndex(cubemapIndex);
    equiImageDescriptorBuffer->freeDescriptorBufferIndex(equipIndex);


    auto end0 = std::chrono::system_clock::now();
    auto elapsed0 = std::chrono::duration_cast<std::chrono::microseconds>(end0 - start);
    fmt::print("Environment Map: {} | Cubemap {:.1f}ms | ", file::getFileName(path), elapsed0.count() / 1000.0f);
    newEnvironmentEntry.specDiffCubemap = resourceManager.createCubemapImage(SPECULAR_PREFILTERED_BASE_EXTENTS, VK_FORMAT_R32G32B32A32_SFLOAT,
                                                                             VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                                                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                                                             VK_IMAGE_USAGE_STORAGE_BIT, true);


    EnvironmentCubemap specDiffCubemap = {};
    specDiffCubemap.image = newEnvironmentEntry.specDiffCubemap->image;
    specDiffCubemap.imageFormat = newEnvironmentEntry.specDiffCubemap->imageFormat;
    specDiffCubemap.cubemapImageViews = std::vector<EnvironmentCubemapView>(ENVIRONMENT_MAP_MIP_COUNT);

    for (int32_t i = 0; i < ENVIRONMENT_MAP_MIP_COUNT; i++) {
        EnvironmentCubemapView cubemapImageView{};
        VkImageViewCreateInfo viewInfo = vk_helpers::cubemapViewCreateInfo(specDiffCubemap.imageFormat, specDiffCubemap.image,
                                                                           VK_IMAGE_ASPECT_COLOR_BIT);
        viewInfo.subresourceRange.baseMipLevel = i;
        cubemapImageView.imageView = resourceManager.createResource<ImageView>(viewInfo);

        auto length = static_cast<uint32_t>(SPECULAR_PREFILTERED_BASE_EXTENTS.width / pow(2, i)); // w and h always equal (assumption)
        cubemapImageView.imageExtent = {length, length, 1};
        // 0, 0.25, 0.5, 0.75, 1.0, diffuse
        float roughness = i >= SPECULAR_PREFILTERED_MIP_LEVELS
                              ? -1.0f
                              : static_cast<float>(i) / static_cast<float>(SPECULAR_PREFILTERED_MIP_LEVELS - 1);

        cubemapImageView.roughness = roughness;

        VkDescriptorImageInfo prefilteredCubemapStorage{};
        prefilteredCubemapStorage.imageView = cubemapImageView.imageView->imageView;
        prefilteredCubemapStorage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        std::vector<DescriptorImageData> prefilteredCubemapStorageDescriptor = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, prefilteredCubemapStorage}};

        int32_t descriptorBufferIndex = cubemapStorageDescriptorBuffer->setupData(prefilteredCubemapStorageDescriptor);
        cubemapImageView.descriptorBufferIndex = descriptorBufferIndex;
        specDiffCubemap.cubemapImageViews[i] = std::move(cubemapImageView);
    }

    immediate.submit([&](VkCommandBuffer cmd) {
        vk_helpers::imageBarrier(cmd, specDiffCubemap.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                 VK_IMAGE_ASPECT_COLOR_BIT);


        VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[2];
        descriptorBufferBindingInfo[0] = cubemapDescriptorBuffer->getBindingInfo();
        descriptorBufferBindingInfo[1] = cubemapStorageDescriptorBuffer->getBindingInfo();

        constexpr uint32_t _cubemapIndex = 0;
        constexpr uint32_t storageCubemapIndex = 1;
        const VkDeviceSize sampleOffset = environmentMapIndex * cubemapDescriptorBuffer->getDescriptorBufferSize();

        // Diffuse
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapToDiffusePipeline->pipeline);
            vkCmdBindDescriptorBuffersEXT(cmd, 2, descriptorBufferBindingInfo);

            assert(specDiffCubemap.cubemapImageViews.size() == ENVIRONMENT_MAP_MIP_COUNT);
            vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapToDiffusePipelineLayout->layout, 0, 1, &_cubemapIndex,
                                               &sampleOffset);
            const EnvironmentCubemapView& diffuse = specDiffCubemap.cubemapImageViews[ENVIRONMENT_MAP_MIP_COUNT - 1];
            const VkDeviceSize diffuseMapOffset = cubemapStorageDescriptorBuffer->getDescriptorBufferSize() * diffuse.descriptorBufferIndex;
            vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapToDiffusePipelineLayout->layout, 1, 1, &storageCubemapIndex,
                                               &diffuseMapOffset);

            CubeToDiffusePushConstantData pushData{};
            pushData.sampleDelta = DIFFUSE_SAMPLE_DELTA;
            vkCmdPushConstants(cmd, cubemapToDiffusePipelineLayout->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CubeToDiffusePushConstantData),
                               &pushData);

            // width and height should be 32 (if extents are 512), don't need bounds check in shader code
            const auto xDispatch = static_cast<uint32_t>(std::ceil(diffuse.imageExtent.width / 8.0f));
            const auto yDispatch = static_cast<uint32_t>(std::ceil(diffuse.imageExtent.height / 8.0f));

            vkCmdDispatch(cmd, xDispatch, yDispatch, 6);
        }


        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapToSpecularPipeline->pipeline);

        // Specular
        {
            vkCmdBindDescriptorBuffersEXT(cmd, 2, descriptorBufferBindingInfo);
            vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapToSpecularPipelineLayout->layout, 0, 1, &_cubemapIndex,
                                               &sampleOffset);

            for (int32_t i = 0; i < SPECULAR_PREFILTERED_MIP_LEVELS; i++) {
                const EnvironmentCubemapView& current = specDiffCubemap.cubemapImageViews[i];

                VkDeviceSize irradiancemap_offset = cubemapStorageDescriptorBuffer->getDescriptorBufferSize() * current.descriptorBufferIndex;
                vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapToSpecularPipelineLayout->layout, 1, 1,
                                                   &storageCubemapIndex,
                                                   &irradiancemap_offset);

                CubeToPrefilteredConstantData pushData{};
                pushData.roughness = current.roughness;
                pushData.imageWidth = current.imageExtent.width;
                pushData.imageHeight = current.imageExtent.height;
                pushData.sampleCount = static_cast<uint32_t>(SPECULAR_SAMPLE_COUNT);
                vkCmdPushConstants(cmd, cubemapToSpecularPipelineLayout->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CubeToDiffusePushConstantData),
                                   &pushData);

                const auto xDispatch = static_cast<uint32_t>(std::ceil(current.imageExtent.width / 8.0f));
                const auto yDispatch = static_cast<uint32_t>(std::ceil(current.imageExtent.height / 8.0f));


                vkCmdDispatch(cmd, xDispatch, yDispatch, 6);
            }
        }


        vk_helpers::imageBarrier(cmd, specDiffCubemap.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                 VK_IMAGE_ASPECT_COLOR_BIT);
    });


    // can safely destroy all the storage mip level image views since we don't plan on writing to them anymore
    for (EnvironmentCubemapView& specDiffView : specDiffCubemap.cubemapImageViews) {
        resourceManager.destroyResourceImmediate(std::move(specDiffView.imageView));
    }
    specDiffCubemap.cubemapImageViews.clear();
    cubemapStorageDescriptorBuffer->freeAllDescriptorBufferIndices();

    newEnvironmentEntry.specDiffCubemap->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    auto end1 = std::chrono::system_clock::now();
    auto elapsed1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - end0);
    fmt::print("D/S Map {:.1f}ms | ", elapsed1.count() / 1000.0f);


    VkDescriptorImageInfo diffSpecDescriptorInfo{};
    diffSpecDescriptorInfo.sampler = sampler->sampler;
    diffSpecDescriptorInfo.imageView = newEnvironmentEntry.specDiffCubemap->imageView;
    diffSpecDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo lutDescriptorInfo{};
    lutDescriptorInfo.sampler = sampler->sampler;
    lutDescriptorInfo.imageView = lutImage->imageView;
    lutDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    std::vector<DescriptorImageData> combinedDescriptor2 = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, diffSpecDescriptorInfo},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, lutDescriptorInfo}
    };

    diffSpecMapDescriptorBuffer->setupData(combinedDescriptor2, environmentMapIndex);

    environmentMaps[environmentMapIndex] = std::move(newEnvironmentEntry);
    activeEnvironmentMapNames.insert({environmentMapIndex, name});

    auto end3 = std::chrono::system_clock::now();
    auto elapsed3 = std::chrono::duration_cast<std::chrono::microseconds>(end3 - start);
    fmt::print("Total {:.1f}ms\n", elapsed3.count() / 1000.0f);
}
}
