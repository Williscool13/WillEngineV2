//
// Created by William on 2024-08-25.
//

#include "environment.h"

#include <cassert>
#include <../../extern/stb/stb_image.h>
//#include <chrono.h>
#include "engine.h"

const VkExtent3D Environment::specularPrefilteredBaseExtents = {512, 512, 1};
const VkExtent3D Environment::lutImageExtent = {512, 512, 1};
const char* Environment::defaultEquiPath = "assets/environments/meadow_4k.hdr";

int Environment::useCount = 0;
VkDescriptorSetLayout Environment::equiImageDescriptorSetLayout = VK_NULL_HANDLE;
VkDescriptorSetLayout Environment::cubemapStorageDescriptorSetLayout = VK_NULL_HANDLE;
VkDescriptorSetLayout Environment::cubemapDescriptorSetLayout = VK_NULL_HANDLE;
VkDescriptorSetLayout Environment::lutDescriptorSetLayout = VK_NULL_HANDLE;
/**
 * Diff/Spec Irradiance Cubemap (LOD 1-4 spec, LOD 5 diff), and 2D-LUT
 */
VkDescriptorSetLayout Environment::environmentMapDescriptorSetLayout = VK_NULL_HANDLE;

VkPipelineLayout Environment::equiToCubemapPipelineLayout = VK_NULL_HANDLE;
VkPipeline Environment::equiToCubemapPipeline = VK_NULL_HANDLE;
VkPipelineLayout Environment::cubemapToDiffusePipelineLayout = VK_NULL_HANDLE;
VkPipeline Environment::cubemapToDiffusePipeline = VK_NULL_HANDLE;
VkPipelineLayout Environment::cubemapToSpecularPipelineLayout = VK_NULL_HANDLE;
VkPipeline Environment::cubemapToSpecularPipeline = VK_NULL_HANDLE;
VkPipelineLayout Environment::lutPipelineLayout = VK_NULL_HANDLE;
VkPipeline Environment::lutPipeline = VK_NULL_HANDLE;
DescriptorBufferSampler Environment::lutDescriptorBuffer = DescriptorBufferSampler();

AllocatedImage Environment::lutImage = {};

bool Environment::layoutsCreated = false;

Environment::Environment(Engine* creator)
{
    this->creator = creator;
    device = creator->getDevice();
    allocator = creator->getAllocator();

    VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

    sampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampl.minLod = 0;
    sampl.maxLod = VK_LOD_CLAMP_NONE;
    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR;

    sampl.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampl.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampl.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    vkCreateSampler(device, &sampl, nullptr, &sampler);

    if (!layoutsCreated) {
        initializeStatics(creator);
        layoutsCreated = true;
    }

    equiImageDescriptorBuffer = DescriptorBufferSampler(creator->getInstance(), creator->getDevice()
                                                        , creator->getPhysicalDevice(), creator->getAllocator(), equiImageDescriptorSetLayout, 1);
    // 0 is original cubemap, 1 is diff irr, 2 is spec pref, 3 to 13 is for 10 mip levels of spec pref
    cubemapStorageDescriptorBuffer = DescriptorBufferSampler(creator->getInstance(), creator->getDevice()
                                                             , creator->getPhysicalDevice(), creator->getAllocator(),
                                                             cubemapStorageDescriptorSetLayout, 12);

    // sample cubemap
    cubemapDescriptorBuffer = DescriptorBufferSampler(creator->getInstance(), creator->getDevice()
                                                      , creator->getPhysicalDevice(), creator->getAllocator(), cubemapDescriptorSetLayout,
                                                      MAX_ENVIRONMENT_MAPS);
    diffSpecMapDescriptorBuffer = DescriptorBufferSampler(creator->getInstance(), creator->getDevice()
                                                           , creator->getPhysicalDevice(), creator->getAllocator(), environmentMapDescriptorSetLayout,
                                                           MAX_ENVIRONMENT_MAPS);


    useCount++;
}

Environment::~Environment()
{
    useCount--;
    if (useCount == 0 && layoutsCreated) {
        vkDestroyDescriptorSetLayout(device, equiImageDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, cubemapStorageDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, cubemapDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, lutDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, environmentMapDescriptorSetLayout, nullptr);
        equiImageDescriptorSetLayout = VK_NULL_HANDLE;
        cubemapStorageDescriptorSetLayout = VK_NULL_HANDLE;
        cubemapDescriptorSetLayout = VK_NULL_HANDLE;
        lutDescriptorSetLayout = VK_NULL_HANDLE;
        environmentMapDescriptorSetLayout = VK_NULL_HANDLE;

        vkDestroyPipelineLayout(device, equiToCubemapPipelineLayout, nullptr);
        vkDestroyPipeline(device, equiToCubemapPipeline, nullptr);
        vkDestroyPipelineLayout(device, cubemapToDiffusePipelineLayout, nullptr);
        vkDestroyPipeline(device, cubemapToDiffusePipeline, nullptr);
        vkDestroyPipelineLayout(device, cubemapToSpecularPipelineLayout, nullptr);
        vkDestroyPipeline(device, cubemapToSpecularPipeline, nullptr);
        vkDestroyPipelineLayout(device, lutPipelineLayout, nullptr);
        vkDestroyPipeline(device, lutPipeline, nullptr);
        equiToCubemapPipelineLayout = VK_NULL_HANDLE;
        equiToCubemapPipeline = VK_NULL_HANDLE;
        cubemapToDiffusePipelineLayout = VK_NULL_HANDLE;
        cubemapToDiffusePipeline = VK_NULL_HANDLE;
        cubemapToSpecularPipelineLayout = VK_NULL_HANDLE;
        cubemapToSpecularPipeline = VK_NULL_HANDLE;

        lutDescriptorBuffer.destroy(device, allocator);
        creator->destroyImage(lutImage);
        lutImage = {};
        lutDescriptorBuffer = DescriptorBufferSampler();

        layoutsCreated = false;
    }

    vkDestroySampler(device, sampler, nullptr);

    cubemapStorageDescriptorBuffer.destroy(device, allocator);
    cubemapDescriptorBuffer.destroy(device, allocator);
    equiImageDescriptorBuffer.destroy(device, allocator);
    diffSpecMapDescriptorBuffer.destroy(device, allocator);

    for (EnvironmentMapData& envMap: environmentMaps) {
        creator->destroyImage(envMap.cubemapImage);
        creator->destroyImage(envMap.specDiffCubemap);
    }
}

void Environment::loadCubemap(const char* path, int environmentMapIndex)
{
    auto start = std::chrono::system_clock::now();
    std::filesystem::path cubemapPath{path};

    EnvironmentMapData newEnvMapData{};
    newEnvMapData.sourcePath = cubemapPath.string();
    VkExtent3D extents = {cubemapResolution, cubemapResolution, 1};


    assert(environmentMapIndex < MAX_ENVIRONMENT_MAPS && environmentMapIndex >= 0);
    AllocatedImage equiImage;
    int width, height, channels;
    float* imageData = stbi_loadf(newEnvMapData.sourcePath.c_str(), &width, &height, &channels, 4);
    if (imageData) {
        equiImage = creator->createImage(imageData, width * height * 4 * sizeof(float), VkExtent3D{(uint32_t) width, (uint32_t) height, 1},
                                         VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, true);
        stbi_image_free(imageData);
    } else {
        fmt::print("Failed to load Equirectangular Image ({})\n", path);
    }
    assert(equiImage.imageExtent.width % 4 == 0);
    cubemapResolution = equiImage.imageExtent.width / 4;


    // Equi -> Cubemap - recreate in case resolution changed
    newEnvMapData.cubemapImage = creator->createCubemap(extents, VK_FORMAT_R32G32B32A32_SFLOAT,
                                                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                                        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);

    // add new cubemap image to descriptor buffer
    {}
    VkDescriptorImageInfo equiImageDescriptorInfo{};
    equiImageDescriptorInfo.sampler = sampler;
    equiImageDescriptorInfo.imageView = equiImage.imageView;
    equiImageDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // needs to match the order of the bindings in the layout
    std::vector<DescriptorImageData> combined_descriptor = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, equiImageDescriptorInfo}};
    equiImageDescriptorBuffer.setupData(device, combined_descriptor);


    VkDescriptorImageInfo cubemapDescriptor{};
    cubemapDescriptor.sampler = sampler;
    cubemapDescriptor.imageView = newEnvMapData.cubemapImage.imageView;
    cubemapDescriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    std::vector<DescriptorImageData> cubemapStorageDescriptor = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, cubemapDescriptor}};
    int cubemapIndex = cubemapStorageDescriptorBuffer.setupData(device, cubemapStorageDescriptor);
    cubemapDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    std::vector<DescriptorImageData> cubemapSamplerDescriptor = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, cubemapDescriptor}};
    cubemapDescriptorBuffer.setupData(device, cubemapSamplerDescriptor, environmentMapIndex);


    equiToCubemapImmediate(newEnvMapData.cubemapImage, cubemapIndex);

    // can safely destroy the cubemap image view in the storage buffer
    creator->destroyImage(equiImage);
    cubemapStorageDescriptorBuffer.freeDescriptorBuffer(cubemapIndex);
    equiImageDescriptorBuffer.freeDescriptorBuffer(0); // always 0


    auto end0 = std::chrono::system_clock::now();
    auto elapsed0 = std::chrono::duration_cast<std::chrono::microseconds>(end0 - start);
    fmt::print("Environment Map: {} | Cubemap {}ms | ",path, elapsed0.count() / 1000.0f);
    newEnvMapData.specDiffCubemap = creator->createCubemap(specularPrefilteredBaseExtents, VK_FORMAT_R32G32B32A32_SFLOAT,
                                                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                                           VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                                           VK_IMAGE_USAGE_STORAGE_BIT, true);


    int diffuseIndex = diffuseIrradianceMipLevel;

    AllocatedCubemap specDiffCubemap = {};
    specDiffCubemap.allocatedImage = newEnvMapData.specDiffCubemap;
    specDiffCubemap.mipLevels = specularPrefilteredMipLevels;
    specDiffCubemap.cubemapImageViews = std::vector<CubemapImageView>(specularPrefilteredMipLevels);
    assert(specularPrefilteredBaseExtents.width == specularPrefilteredBaseExtents.height);

    for (int i = 0; i < specularPrefilteredMipLevels; i++) {
        CubemapImageView cubemapImageView{};
        VkImageViewCreateInfo view_info = vk_helpers::cubemapViewCreateInfo(newEnvMapData.specDiffCubemap.imageFormat,
                                                                            newEnvMapData.specDiffCubemap.image, VK_IMAGE_ASPECT_COLOR_BIT);
        view_info.subresourceRange.baseMipLevel = i;
        VK_CHECK(vkCreateImageView(device, &view_info, nullptr, &cubemapImageView.imageView));

        auto length = static_cast<uint32_t>(specularPrefilteredBaseExtents.width / pow(2, i)); // w and h always equal
        cubemapImageView.imageExtent = {length, length, 1};
        float roughness{};
        int j = i;
        if (i > 5) { j = i - 1; }
        if (i == 5) { roughness = -1; } // diffuse irradiance map
        else { roughness = static_cast<float>(j) / static_cast<float>(specularPrefilteredMipLevels - 2); }

        cubemapImageView.roughness = roughness;

        VkDescriptorImageInfo prefilteredCubemapStorage{};
        prefilteredCubemapStorage.sampler = nullptr; // sampler not actually used in storage image
        prefilteredCubemapStorage.imageView = cubemapImageView.imageView;
        prefilteredCubemapStorage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        std::vector<DescriptorImageData> prefilteredCubemapStorageDescriptor = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, prefilteredCubemapStorage}};

        int descriptorBufferIndex = cubemapStorageDescriptorBuffer.setupData(device, prefilteredCubemapStorageDescriptor);
        cubemapImageView.descriptorBufferIndex = descriptorBufferIndex;

        specDiffCubemap.cubemapImageViews[i] = cubemapImageView;
    }

    cubemapToDiffuseSpecularImmediate(specDiffCubemap, environmentMapIndex);
    // can safely destroy all the mip level image views
    for (int i = 0; i < specDiffCubemap.mipLevels; i++) {
        vkDestroyImageView(device, specDiffCubemap.cubemapImageViews[i].imageView, nullptr);
        cubemapStorageDescriptorBuffer.freeDescriptorBuffer(specDiffCubemap.cubemapImageViews[i].descriptorBufferIndex);
    }


    auto end1 = std::chrono::system_clock::now();
    auto elapsed1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - end0);
    fmt::print("Diff/Spec Maps {}ms | ", elapsed1.count() / 1000.0f);


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

    diffSpecMapDescriptorBuffer.setupData(device, combinedDescriptor2, environmentMapIndex);

    environmentMaps[environmentMapIndex] = newEnvMapData;


    auto end3 = std::chrono::system_clock::now();
    auto elapsed3 = std::chrono::duration_cast<std::chrono::microseconds>(end3 - start);

    fmt::print("Total Time {}ms\n", elapsed3.count() / 1000.0f);
}

void Environment::equiToCubemapImmediate(AllocatedImage& _cubemapImage, int _cubemapStorageDescriptorIndex)
{
    creator->immediateSubmit([&](VkCommandBuffer cmd) {
        VkExtent3D extents = {cubemapResolution, cubemapResolution, 1};

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, equiToCubemapPipeline);

        vk_helpers::transitionImage(cmd, _cubemapImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

        VkDescriptorBufferBindingInfoEXT descriptor_buffer_binding_info[2] = {
            equiImageDescriptorBuffer.getDescriptorBufferBindingInfo(),
            cubemapStorageDescriptorBuffer.getDescriptorBufferBindingInfo(),
        };

        vkCmdBindDescriptorBuffersEXT(cmd, 2, descriptor_buffer_binding_info);
        uint32_t equiImage_index = 0;
        uint32_t cubemap_index = 1;

        VkDeviceSize offset = 0;
        VkDeviceSize cubemapOffset = _cubemapStorageDescriptorIndex * cubemapStorageDescriptorBuffer.getDescriptorBufferSize();
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, equiToCubemapPipelineLayout
                                           , 0, 1, &equiImage_index, &offset);
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, equiToCubemapPipelineLayout
                                           , 1, 1, &cubemap_index, &cubemapOffset);


        EquiToCubePushConstantData pushData{};
        pushData.flipY = flipY;
        vkCmdPushConstants(cmd, equiToCubemapPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(EquiToCubePushConstantData), &pushData);

        assert(extents.width % 16 == 0 && extents.height % 16 == 0);
        vkCmdDispatch(cmd, extents.width / 16, extents.height / 16, 6);

        vk_helpers::transitionImage(cmd, _cubemapImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    });
}

void Environment::cubemapToDiffuseSpecularImmediate(AllocatedCubemap& cubemapMips, int _cubemapSampleDescriptorIndex)
{
    creator->immediateSubmit([&](VkCommandBuffer cmd) {
        vk_helpers::transitionImage(cmd, cubemapMips.allocatedImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);


        VkDescriptorBufferBindingInfoEXT descriptor_buffer_binding_info[2] = {
            cubemapDescriptorBuffer.getDescriptorBufferBindingInfo(),
            cubemapStorageDescriptorBuffer.getDescriptorBufferBindingInfo(),
        };
        uint32_t cubemap_index = 0;
        uint32_t storage_cubemap_index = 1;
        VkDeviceSize sample_offset = _cubemapSampleDescriptorIndex * cubemapDescriptorBuffer.getDescriptorBufferSize();
        VkDeviceSize zero_offset = 0;
        // Diffuse
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapToDiffusePipeline);
            vkCmdBindDescriptorBuffersEXT(cmd, 2, descriptor_buffer_binding_info);
            vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapToDiffusePipelineLayout, 0, 1, &cubemap_index,
                                               &sample_offset);

            CubemapImageView& diffuse = cubemapMips.cubemapImageViews[5];
            assert(diffuse.roughness == -1);

            VkDeviceSize diffusemap_offset = cubemapStorageDescriptorBuffer.getDescriptorBufferSize() * diffuse.descriptorBufferIndex;
            vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapToDiffusePipelineLayout, 1, 1, &storage_cubemap_index,
                                               &diffusemap_offset);

            CubeToDiffusePushConstantData pushData{};
            pushData.sampleDelta = diffuse_sample_delta;
            vkCmdPushConstants(cmd, cubemapToDiffusePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CubeToDiffusePushConstantData), &pushData);

            // width and height should be 32, dont need bounds check in shader code
            uint32_t xDispatch = static_cast<uint32_t>(std::ceil(diffuse.imageExtent.width / 8.0f));
            uint32_t yDispatch = static_cast<uint32_t>(std::ceil(diffuse.imageExtent.height / 8.0f));

            vkCmdDispatch(cmd, xDispatch, yDispatch, 6);
        }


        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapToSpecularPipeline);
        vkCmdBindDescriptorBuffersEXT(cmd, 2, descriptor_buffer_binding_info);
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapToSpecularPipelineLayout, 0, 1, &cubemap_index,
                                           &sample_offset);

        for (int i = 0; i < specularPrefilteredMipLevels; i++) {
            if (i == 5) continue; // skip the diffuse map mip level
            CubemapImageView& current = cubemapMips.cubemapImageViews[i];

            VkDeviceSize irradiancemap_offset = cubemapStorageDescriptorBuffer.getDescriptorBufferSize() * current.descriptorBufferIndex;
            vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cubemapToSpecularPipelineLayout, 1, 1, &storage_cubemap_index,
                                               &irradiancemap_offset);

            CubeToPrefilteredConstantData pushData{};
            pushData.roughness = current.roughness;
            pushData.imageWidth = current.imageExtent.width;
            pushData.imageHeight = current.imageExtent.height;
            pushData.sampleCount = static_cast<uint32_t>(specular_sample_count);
            vkCmdPushConstants(cmd, cubemapToSpecularPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CubeToDiffusePushConstantData),
                               &pushData);

            uint32_t xDispatch = static_cast<uint32_t>(std::ceil(current.imageExtent.width / 8.0f));
            uint32_t yDispatch = static_cast<uint32_t>(std::ceil(current.imageExtent.height / 8.0f));


            vkCmdDispatch(cmd, xDispatch, yDispatch, 6);
        }


        vk_helpers::transitionImage(cmd, cubemapMips.allocatedImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    });
}

void Environment::generateLutImmediate(Engine* engine)
{
    engine->immediateSubmit([&](VkCommandBuffer cmd) {
        vk_helpers::transitionImage(cmd, lutImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, lutPipeline);

        VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[1] = {lutDescriptorBuffer.getDescriptorBufferBindingInfo()};

        uint32_t lutIndex = 0;
        VkDeviceSize zeroOffset = 0;

        vkCmdBindDescriptorBuffersEXT(cmd, 1, descriptorBufferBindingInfo);
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, lutPipelineLayout, 0, 1, &lutIndex, &zeroOffset);

        // must be divisible by 16, dont want to bother bounds checking in shader code
        assert(lutImageExtent.width % 8 == 0 && lutImageExtent.height % 8 == 0);
        uint32_t x_disp = lutImageExtent.width / 8;
        uint32_t y_disp = lutImageExtent.height / 8;
        uint32_t z_disp = 1;
        vkCmdDispatch(cmd, x_disp, y_disp, z_disp);

        vk_helpers::transitionImage(cmd, lutImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    });
}

void Environment::initializeStatics(Engine* engine)
{
    //  Equirectangular Image Descriptor Set Layout
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        equiImageDescriptorSetLayout = layoutBuilder.build(engine->getDevice(), VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
                                                           , nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }
    //  Cubemap Descriptor Set Layout
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        cubemapStorageDescriptorSetLayout = layoutBuilder.build(engine->getDevice(), VK_SHADER_STAGE_COMPUTE_BIT
                                                                , nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }
    //  SAMPLER cubemaps
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        cubemapDescriptorSetLayout = layoutBuilder.build(engine->getDevice(), VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
                                                         , nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }
    // LUT generation
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        lutDescriptorSetLayout = layoutBuilder.build(engine->getDevice(), VK_SHADER_STAGE_COMPUTE_BIT
                                                     , nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }

    // Full Cubemap Descriptor - Used in actual code
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // 1 cubemap  - diffuse/spec
        layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // 1 2d image - lut
        environmentMapDescriptorSetLayout = layoutBuilder.build(engine->getDevice(), VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
                                                                , nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }


    // equi to cubemap pipeline
    {
        VkPushConstantRange pushConstantRange = {};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(EquiToCubePushConstantData);

        VkDescriptorSetLayout layouts[]{equiImageDescriptorSetLayout, cubemapStorageDescriptorSetLayout};

        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 2;
        layout_info.pSetLayouts = layouts;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &pushConstantRange;

        VK_CHECK(vkCreatePipelineLayout(engine->getDevice(), &layout_info, nullptr, &equiToCubemapPipelineLayout));


        VkShaderModule computeShader;
        if (!vk_helpers::loadShaderModule("shaders/environment/equitoface.comp.spv", engine->getDevice(), &computeShader)) {
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

        VK_CHECK(vkCreateComputePipelines(engine->getDevice(), VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &equiToCubemapPipeline));

        vkDestroyShaderModule(engine->getDevice(), computeShader, nullptr);
    }

    // cubemap to diffuse pipeline
    {
        VkDescriptorSetLayout layouts[]{cubemapDescriptorSetLayout, cubemapStorageDescriptorSetLayout};

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

        VK_CHECK(vkCreatePipelineLayout(engine->getDevice(), &layout_info, nullptr, &cubemapToDiffusePipelineLayout));

        VkShaderModule computeShader;
        if (!vk_helpers::loadShaderModule("shaders/environment/cubetodiffirra.comp.spv", engine->getDevice(), &computeShader)) {
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

        VK_CHECK(vkCreateComputePipelines(engine->getDevice(), VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &cubemapToDiffusePipeline));

        vkDestroyShaderModule(engine->getDevice(), computeShader, nullptr);
    }

    // cubemap to specular pipeline
    {
        VkDescriptorSetLayout layouts[]{cubemapDescriptorSetLayout, cubemapStorageDescriptorSetLayout};

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

        VK_CHECK(vkCreatePipelineLayout(engine->getDevice(), &layout_info, nullptr, &cubemapToSpecularPipelineLayout));

        VkShaderModule computeShader;
        if (!vk_helpers::loadShaderModule("shaders/environment/cubetospecprefilter.comp.spv", engine->getDevice(), &computeShader)) {
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

        VK_CHECK(vkCreateComputePipelines(engine->getDevice(), VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &cubemapToSpecularPipeline));

        vkDestroyShaderModule(engine->getDevice(), computeShader, nullptr);
    }

    // lut generation pipeline
    {
        VkDescriptorSetLayout layouts[1]{lutDescriptorSetLayout};

        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = layouts;
        layout_info.pushConstantRangeCount = 0;
        layout_info.pPushConstantRanges = nullptr;

        VK_CHECK(vkCreatePipelineLayout(engine->getDevice(), &layout_info, nullptr, &lutPipelineLayout));

        VkShaderModule computeShader;
        if (!vk_helpers::loadShaderModule("shaders/environment/brdflut.comp.spv", engine->getDevice(), &computeShader)) {
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

        VK_CHECK(vkCreateComputePipelines(engine->getDevice(), VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &lutPipeline));

        vkDestroyShaderModule(engine->getDevice(), computeShader, nullptr);
    }

    // Create LUT here, because its the same for all environment maps
    {
        lutDescriptorBuffer = DescriptorBufferSampler(engine->getInstance(), engine->getDevice()
                                                      , engine->getPhysicalDevice(), engine->getAllocator(), lutDescriptorSetLayout, 1);

        lutImage = engine->createImage(lutImageExtent, VK_FORMAT_R32G32_SFLOAT,
                                       VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

        VkDescriptorImageInfo lutDescriptorInfo{};
        lutDescriptorInfo.sampler = nullptr; // not sampled (storage)
        lutDescriptorInfo.imageView = lutImage.imageView;
        lutDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        std::vector<DescriptorImageData> descriptor = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, lutDescriptorInfo}};


        lutDescriptorBuffer.setupData(engine->getDevice(), descriptor); // index 0, obviously
        generateLutImmediate(engine);
    }
}
