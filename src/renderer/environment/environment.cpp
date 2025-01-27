//
// Created by William on 2025-01-25.
//

#include "environment.h"

#include <stb_image.h>
#include <vulkan/vulkan_core.h>

#include "environment_constants.h"
#include "src/renderer/resource_manager.h"
#include "src/util/file.h"

will_engine::environment::Environment::Environment(ResourceManager& resourceManager, ImmediateSubmitter& immediate)
    : resourceManager(resourceManager), immediate(immediate)
{
    // Descriptor Set Layouts
    {
        //  Equirectangular Image Descriptor Set Layout
        {
            DescriptorLayoutBuilder layoutBuilder;
            layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            equiImageLayout = resourceManager.createDescriptorSetLayout(layoutBuilder, static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT),
                                                                        VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
        }

        //  (Storage) Cubemap Descriptor Set Layout
        {
            DescriptorLayoutBuilder layoutBuilder;
            layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
            cubemapStorageLayout = resourceManager.createDescriptorSetLayout(layoutBuilder, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
        }
        //  (Sampler) Cubemap Descriptor Set Layout
        {
            DescriptorLayoutBuilder layoutBuilder;
            layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            cubemapSamplerLayout = resourceManager.createDescriptorSetLayout(layoutBuilder, static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
                                                                             VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
        }
        // LUT generation
        {
            DescriptorLayoutBuilder layoutBuilder;
            layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
            lutLayout = resourceManager.createDescriptorSetLayout(layoutBuilder, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
        }

        // Full Cubemap Descriptor - contains PBR IBL data
        {
            DescriptorLayoutBuilder layoutBuilder;
            layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // 1 cubemap  - diffuse/spec
            layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // 1 2d image - lut
            environmentIBLLayout = resourceManager.createDescriptorSetLayout(layoutBuilder, static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT),
                                                                             VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
        }
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

    sampler = resourceManager.createSampler(samplerInfo);

    // Equi -> Cubemap Pipeline
    {
        VkDescriptorSetLayout layouts[]{equiImageLayout, cubemapStorageLayout};

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 2;
        layoutInfo.pSetLayouts = layouts;
        layoutInfo.pushConstantRangeCount = 0;
        layoutInfo.pPushConstantRanges = nullptr;

        equiToCubemapPipelineLayout = resourceManager.createPipelineLayout(layoutInfo);

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
        computePipelineCreateInfo.layout = equiToCubemapPipelineLayout;
        computePipelineCreateInfo.stage = stageInfo;
        computePipelineCreateInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

        equiToCubemapPipeline = resourceManager.createComputePipeline(computePipelineCreateInfo);
        resourceManager.destroyShaderModule(computeShader);
    }

    // Cubemap -> Diff Pipeline
    {
        VkDescriptorSetLayout layouts[]{cubemapSamplerLayout, cubemapStorageLayout};

        VkPushConstantRange pushConstantRange = {};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(CubeToDiffusePushConstantData);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 2;
        layoutInfo.pSetLayouts = layouts;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstantRange;

        cubemapToDiffusePipelineLayout = resourceManager.createPipelineLayout(layoutInfo);

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
        computePipelineCreateInfo.layout = cubemapToDiffusePipelineLayout;
        computePipelineCreateInfo.stage = stageInfo;
        computePipelineCreateInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

        cubemapToDiffusePipeline = resourceManager.createComputePipeline(computePipelineCreateInfo);
        resourceManager.destroyShaderModule(computeShader);
    }

    // Cubemap -> Spec Pipeline
    {
        VkDescriptorSetLayout layouts[]{cubemapSamplerLayout, cubemapStorageLayout};

        VkPushConstantRange pushConstantRange = {};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(CubeToPrefilteredConstantData);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 2;
        layoutInfo.pSetLayouts = layouts;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstantRange;

        cubemapToSpecularPipelineLayout = resourceManager.createPipelineLayout(layoutInfo);

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
        computePipelineCreateInfo.layout = cubemapToSpecularPipelineLayout;
        computePipelineCreateInfo.stage = stageInfo;
        computePipelineCreateInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

        cubemapToSpecularPipeline = resourceManager.createComputePipeline(computePipelineCreateInfo);
        resourceManager.destroyShaderModule(computeShader);
    }

    // LUT Generation
    {
        VkDescriptorSetLayout layouts[1]{lutLayout};

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = layouts;
        layoutInfo.pushConstantRangeCount = 0;
        layoutInfo.pPushConstantRanges = nullptr;

        lutPipelineLayout = resourceManager.createPipelineLayout(layoutInfo);

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
        computePipelineCreateInfo.layout = lutPipelineLayout;
        computePipelineCreateInfo.stage = stageInfo;
        computePipelineCreateInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

        lutPipeline = resourceManager.createComputePipeline(computePipelineCreateInfo);
        resourceManager.destroyShaderModule(computeShader);
    }


    // Execute LUT Generation
    {
        lutDescriptorBuffer = resourceManager.createDescriptorBufferSampler(lutLayout, 1);

        lutImage = resourceManager.createImage(LUT_IMAGE_EXTENT, VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

        VkDescriptorImageInfo lutDescriptorInfo{};
        lutDescriptorInfo.sampler = nullptr; // not sampled (storage)
        lutDescriptorInfo.imageView = lutImage.imageView;
        lutDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        std::vector<DescriptorImageData> descriptor = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, lutDescriptorInfo}};

        resourceManager.setupDescriptorBufferSampler(lutDescriptorBuffer, descriptor, 0);

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

    equiImageDescriptorBuffer = resourceManager.createDescriptorBufferSampler(equiImageLayout, 1);
    // 0 will be filled and cleared for equi->cubemap
    // 0 to 4 and 5 will be filled with spec + diffuse cubemaps but will be cleared right after.
    // This will not exceed 6 size
    cubemapStorageDescriptorBuffer = resourceManager.createDescriptorBufferSampler(cubemapStorageLayout, 6);

    // sample cubemap
    cubemapDescriptorBuffer = resourceManager.createDescriptorBufferSampler(cubemapSamplerLayout, MAX_ENVIRONMENT_MAPS);
    diffSpecMapDescriptorBuffer = resourceManager.createDescriptorBufferSampler(environmentIBLLayout, MAX_ENVIRONMENT_MAPS);
}

will_engine::environment::Environment::~Environment()
{
    resourceManager.destroyPipelineLayout(equiToCubemapPipelineLayout);
    resourceManager.destroyPipelineLayout(cubemapToDiffusePipelineLayout);
    resourceManager.destroyPipelineLayout(cubemapToSpecularPipelineLayout);
    resourceManager.destroyPipelineLayout(lutPipelineLayout);
    resourceManager.destroyPipeline(equiToCubemapPipeline);
    resourceManager.destroyPipeline(cubemapToDiffusePipeline);
    resourceManager.destroyPipeline(cubemapToSpecularPipeline);
    resourceManager.destroyPipeline(lutPipeline);


    resourceManager.destroySampler(sampler);

    resourceManager.destroyDescriptorBuffer(cubemapStorageDescriptorBuffer);
    resourceManager.destroyDescriptorBuffer(cubemapDescriptorBuffer);
    resourceManager.destroyDescriptorBuffer(equiImageDescriptorBuffer);
    resourceManager.destroyDescriptorBuffer(diffSpecMapDescriptorBuffer);
    resourceManager.destroyDescriptorBuffer(lutDescriptorBuffer);

    resourceManager.destroyImage(lutImage);
    lutImage = {};
    for (EnvironmentMapData& envMap : environmentMaps) {
        resourceManager.destroyImage(envMap.cubemapImage);
        resourceManager.destroyImage(envMap.specDiffCubemap);
    }


    resourceManager.destroyDescriptorSetLayout(equiImageLayout);
    resourceManager.destroyDescriptorSetLayout(cubemapStorageLayout);
    resourceManager.destroyDescriptorSetLayout(cubemapSamplerLayout);
    resourceManager.destroyDescriptorSetLayout(lutLayout);
    resourceManager.destroyDescriptorSetLayout(environmentIBLLayout);
}

void will_engine::environment::Environment::loadEnvironment(const char* name, const char* path, int environmentMapIndex)
{
    auto start = std::chrono::system_clock::now();
    std::filesystem::path cubemapPath{path};

    EnvironmentMapData newEnvMapData{};
    newEnvMapData.sourcePath = cubemapPath.string();

    if (environmentMapIndex >= MAX_ENVIRONMENT_MAPS || environmentMapIndex < 0) {
        environmentMapIndex = glm::clamp(environmentMapIndex, 0, MAX_ENVIRONMENT_MAPS - 1);
        fmt::print("Environment map index out of range ({}). Clamping to 0 - {}\n", environmentMapIndex, MAX_ENVIRONMENT_MAPS - 1);
    }

    AllocatedImage equiImage;
    int width, height, channels;
    if (float* imageData = stbi_loadf(newEnvMapData.sourcePath.c_str(), &width, &height, &channels, 4)) {
        equiImage = resourceManager.createImage(imageData, width * height * 4 * sizeof(float), VkExtent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
                                                VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, true);
        stbi_image_free(imageData);
    } else {
        fmt::print("Failed to load Equirectangular Image ({})\n", path);
    }
    if (equiImage.imageExtent.width % 4 != 0 || equiImage.imageExtent.width / 4 != CUBEMAP_RESOLUTION) {
        fmt::print("Dimensions of the equirectangular image is incorrect. Failed to load environment map ({})", path);
        return;
    }


    // Equirectangular -> Cubemap - recreate in case resolution changed
    newEnvMapData.cubemapImage = resourceManager.createCubemap(CUBEMAP_EXTENTS, VK_FORMAT_R32G32B32A32_SFLOAT,
                                                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                                               VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);

    // add new cubemap image to descriptor buffer
    VkDescriptorImageInfo equiImageDescriptorInfo{};
    equiImageDescriptorInfo.sampler = sampler;
    equiImageDescriptorInfo.imageView = equiImage.imageView;
    equiImageDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // needs to match the order of the bindings in the layout
    std::vector<DescriptorImageData> combined_descriptor = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, equiImageDescriptorInfo}};
    int32_t equipIndex = resourceManager.setupDescriptorBufferSampler(equiImageDescriptorBuffer, combined_descriptor);

    VkDescriptorImageInfo cubemapDescriptor{};
    cubemapDescriptor.sampler = sampler;
    cubemapDescriptor.imageView = newEnvMapData.cubemapImage.imageView;
    cubemapDescriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    std::vector<DescriptorImageData> cubemapStorageDescriptor = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, cubemapDescriptor}};
    int32_t cubemapIndex = resourceManager.setupDescriptorBufferSampler(cubemapStorageDescriptorBuffer, cubemapStorageDescriptor);

    cubemapDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    std::vector<DescriptorImageData> cubemapSamplerDescriptor = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, cubemapDescriptor}};
    resourceManager.setupDescriptorBufferSampler(cubemapDescriptorBuffer, cubemapSamplerDescriptor, environmentMapIndex);


    immediate.submit([&](VkCommandBuffer cmd) {
        vk_helpers::transitionImage(cmd, newEnvMapData.cubemapImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, equiToCubemapPipeline);

        VkDescriptorBufferBindingInfoEXT descriptor_buffer_binding_info[2];
        descriptor_buffer_binding_info[0] = equiImageDescriptorBuffer.getDescriptorBufferBindingInfo();
        descriptor_buffer_binding_info[1] = cubemapStorageDescriptorBuffer.getDescriptorBufferBindingInfo();
        vkCmdBindDescriptorBuffersEXT(cmd, 2, descriptor_buffer_binding_info);

        constexpr uint32_t _equiImageIndex = 0;
        constexpr uint32_t _cubemapIndex = 1;
        const VkDeviceSize equiOffset = equipIndex * equiImageDescriptorBuffer.getDescriptorBufferSize();
        const VkDeviceSize cubemapOffset = cubemapIndex * cubemapStorageDescriptorBuffer.getDescriptorBufferSize();
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, equiToCubemapPipelineLayout, 0, 1, &_equiImageIndex, &equiOffset);
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, equiToCubemapPipelineLayout, 1, 1, &_cubemapIndex, &cubemapOffset);

        vkCmdDispatch(cmd, CUBEMAP_EXTENTS.width / 16, CUBEMAP_EXTENTS.height / 16, 6);

        vk_helpers::transitionImage(cmd, newEnvMapData.cubemapImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    });

    // can safely destroy the cubemap image view in the storage buffer
    resourceManager.destroyImage(equiImage);
    cubemapStorageDescriptorBuffer.freeDescriptorBufferIndex(cubemapIndex);
    equiImageDescriptorBuffer.freeDescriptorBufferIndex(equipIndex);


    auto end0 = std::chrono::system_clock::now();
    auto elapsed0 = std::chrono::duration_cast<std::chrono::microseconds>(end0 - start);
    fmt::print("Environment Map: {} | Cubemap {:.1f}ms | ", file::getFileName(path), elapsed0.count() / 1000.0f);
    newEnvMapData.specDiffCubemap = resourceManager.createCubemap(SPECULAR_PREFILTERED_BASE_EXTENTS, VK_FORMAT_R32G32B32A32_SFLOAT,
                                                                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                                                  VK_IMAGE_USAGE_STORAGE_BIT, true);


    AllocatedCubemap specDiffCubemap = {};
    specDiffCubemap.allocatedImage = newEnvMapData.specDiffCubemap;
    specDiffCubemap.cubemapImageViews = std::vector<CubemapImageView>(ENVIRONMENT_MAP_MIP_COUNT);

    for (int i = 0; i < ENVIRONMENT_MAP_MIP_COUNT; i++) {
        CubemapImageView cubemapImageView{};
        VkImageViewCreateInfo viewInfo = vk_helpers::cubemapViewCreateInfo(newEnvMapData.specDiffCubemap.imageFormat, newEnvMapData.specDiffCubemap.image, VK_IMAGE_ASPECT_COLOR_BIT);
        viewInfo.subresourceRange.baseMipLevel = i;
        cubemapImageView.imageView = resourceManager.createImageView(viewInfo);

        auto length = static_cast<uint32_t>(SPECULAR_PREFILTERED_BASE_EXTENTS.width / pow(2, i)); // w and h always equal (assumption)
        cubemapImageView.imageExtent = {length, length, 1};
        // 0, 0.25, 0.5, 0.75, 1.0, diffuse
        float roughness = i >= SPECULAR_PREFILTERED_MIP_LEVELS ? -1.0f : static_cast<float>(i) / static_cast<float>(SPECULAR_PREFILTERED_MIP_LEVELS - 1);

        cubemapImageView.roughness = roughness;

        VkDescriptorImageInfo prefilteredCubemapStorage{};
        prefilteredCubemapStorage.imageView = cubemapImageView.imageView;
        prefilteredCubemapStorage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        std::vector<DescriptorImageData> prefilteredCubemapStorageDescriptor = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, prefilteredCubemapStorage}};

        int32_t descriptorBufferIndex = resourceManager.setupDescriptorBufferSampler(cubemapStorageDescriptorBuffer, prefilteredCubemapStorageDescriptor);
        cubemapImageView.descriptorBufferIndex = descriptorBufferIndex;
        specDiffCubemap.cubemapImageViews[i] = cubemapImageView;
    }

    immediate.submit([&](VkCommandBuffer cmd) {
        vk_helpers::transitionImage(cmd, specDiffCubemap.allocatedImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);


        VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[2];
        descriptorBufferBindingInfo[0] = cubemapDescriptorBuffer.getDescriptorBufferBindingInfo();
        descriptorBufferBindingInfo[1] = cubemapStorageDescriptorBuffer.getDescriptorBufferBindingInfo();

        constexpr uint32_t _cubemapIndex = 0;
        constexpr uint32_t storageCubemapIndex = 1;
        const VkDeviceSize sampleOffset = environmentMapIndex * cubemapDescriptorBuffer.getDescriptorBufferSize();

        // Diffuse
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapToDiffusePipeline);
            vkCmdBindDescriptorBuffersEXT(cmd, 2, descriptorBufferBindingInfo);

            assert(specDiffCubemap.cubemapImageViews.size() == ENVIRONMENT_MAP_MIP_COUNT);
            vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapToDiffusePipelineLayout, 0, 1, &_cubemapIndex, &sampleOffset);
            const CubemapImageView& diffuse = specDiffCubemap.cubemapImageViews[ENVIRONMENT_MAP_MIP_COUNT - 1];
            const VkDeviceSize diffusemapOffset = cubemapStorageDescriptorBuffer.getDescriptorBufferSize() * diffuse.descriptorBufferIndex;
            vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapToDiffusePipelineLayout, 1, 1, &storageCubemapIndex, &diffusemapOffset);

            CubeToDiffusePushConstantData pushData{};
            pushData.sampleDelta = DIFFUSE_SAMPLE_DELTA;
            vkCmdPushConstants(cmd, cubemapToDiffusePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CubeToDiffusePushConstantData), &pushData);

            // width and height should be 32 (if extents are 512), don't need bounds check in shader code
            const auto xDispatch = static_cast<uint32_t>(std::ceil(diffuse.imageExtent.width / 8.0f));
            const auto yDispatch = static_cast<uint32_t>(std::ceil(diffuse.imageExtent.height / 8.0f));

            vkCmdDispatch(cmd, xDispatch, yDispatch, 6);
        }


        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapToSpecularPipeline);

        // Specular
        {
            vkCmdBindDescriptorBuffersEXT(cmd, 2, descriptorBufferBindingInfo);
            vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapToSpecularPipelineLayout, 0, 1, &_cubemapIndex, &sampleOffset);

            for (int i = 0; i < SPECULAR_PREFILTERED_MIP_LEVELS; i++) {
                const CubemapImageView& current = specDiffCubemap.cubemapImageViews[i];

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
        }


        vk_helpers::transitionImage(cmd, specDiffCubemap.allocatedImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    });


    // can safely destroy all the storage mip level image views since we don't plan on writing to them anymore
    for (CubemapImageView specDiffView : specDiffCubemap.cubemapImageViews) {
        resourceManager.destroyImageView(specDiffView.imageView);
    }
    specDiffCubemap.cubemapImageViews.clear();
    cubemapStorageDescriptorBuffer.freeAllDescriptorBufferIndices();

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

    resourceManager.setupDescriptorBufferSampler(diffSpecMapDescriptorBuffer, combinedDescriptor2, environmentMapIndex);

    environmentMaps[environmentMapIndex] = newEnvMapData;
    activeEnvironmentMapNames.insert({environmentMapIndex, name});

    auto end3 = std::chrono::system_clock::now();
    auto elapsed3 = std::chrono::duration_cast<std::chrono::microseconds>(end3 - start);
    fmt::print("Total {:.1f}ms\n", elapsed3.count() / 1000.0f);
}
