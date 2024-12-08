//
// Created by William on 2024-12-08.
//

#include "environment_descriptor_layouts.h"

#include "volk.h"
#include "src/renderer/vk_descriptors.h"

void EnvironmentDescriptorLayouts::createLayouts()
{
    //  Equirectangular Image Descriptor Set Layout
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        equiImageLayout = layoutBuilder.build(context.getDevice(), VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
                                              , nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }
    //  (Storage) Cubemap Descriptor Set Layout
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        cubemapStorageLayout = layoutBuilder.build(context.getDevice(), VK_SHADER_STAGE_COMPUTE_BIT
                                                   , nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }
    //  (Sampler) Cubemap Descriptor Set Layout
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        cubemapSamplerLayout = layoutBuilder.build(context.getDevice(), VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
                                                   , nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }
    // LUT generation
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        lutLayout = layoutBuilder.build(context.getDevice(), VK_SHADER_STAGE_COMPUTE_BIT
                                        , nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }

    // Full Cubemap Descriptor - contains PBR IBL data
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // 1 cubemap  - diffuse/spec
        layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // 1 2d image - lut
        environmentMapLayout = layoutBuilder.build(context.getDevice(), VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
                                                   , nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }
}

void EnvironmentDescriptorLayouts::cleanup() const
{
    if (context.getDevice() != VK_NULL_HANDLE) {
        if (equiImageLayout) vkDestroyDescriptorSetLayout(context.getDevice(), equiImageLayout, nullptr);
        if (cubemapStorageLayout) vkDestroyDescriptorSetLayout(context.getDevice(), cubemapStorageLayout, nullptr);
        if (cubemapSamplerLayout) vkDestroyDescriptorSetLayout(context.getDevice(), cubemapSamplerLayout, nullptr);
        if (lutLayout) vkDestroyDescriptorSetLayout(context.getDevice(), lutLayout, nullptr);
        if (environmentMapLayout) vkDestroyDescriptorSetLayout(context.getDevice(), environmentMapLayout, nullptr);
    }
}
