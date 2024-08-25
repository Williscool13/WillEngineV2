//
// Created by William on 2024-08-25.
//
#include "draw_structure.h"
VkDescriptorSetLayout GltfMetallicRoughness::bufferAddressesDescriptorSetLayout = VK_NULL_HANDLE;
VkDescriptorSetLayout GltfMetallicRoughness::textureDescriptorSetLayout = VK_NULL_HANDLE;
VkDescriptorSetLayout GltfMetallicRoughness::computeCullingDescriptorSetLayout = VK_NULL_HANDLE;
VkPipelineLayout GltfMetallicRoughness::_computeCullingPipelineLayout = VK_NULL_HANDLE;
VkPipeline GltfMetallicRoughness::_computeCullingPipeline = VK_NULL_HANDLE;
bool GltfMetallicRoughness::staticsInitialized = false;
int GltfMetallicRoughness::useCount = 0;


GltfMetallicRoughness::GltfMetallicRoughness(Engine* engine, Scene* scene, const char* vertShaderPath, const char* fragShaderPath)
{
    creator = engine;
    if (!staticsInitialized) {
        initializeStatics(engine);
    }

    useCount++;

}

GltfMetallicRoughness::~GltfMetallicRoughness()
{
    useCount--;
    if (useCount == 0) {
        vkDestroyDescriptorSetLayout(creator->getDevice(), bufferAddressesDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(creator->getDevice(), textureDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(creator->getDevice(), computeCullingDescriptorSetLayout, nullptr);

        vkDestroyPipelineLayout(creator->getDevice(), _computeCullingPipelineLayout, nullptr);
        vkDestroyPipeline(creator->getDevice(), _computeCullingPipeline, nullptr);

        staticsInitialized = false;
    }
}

void GltfMetallicRoughness::buildBuffers()
{

}

void GltfMetallicRoughness::initializeStatics(Engine* engine)
{
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        bufferAddressesDescriptorSetLayout = layoutBuilder.build(engine->getDevice(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
            , nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_SAMPLER, 32); // I think 32 samplers is plenty
        layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 255); // 255 is upper limit of textures

        textureDescriptorSetLayout = layoutBuilder.build(engine->getDevice(), VK_SHADER_STAGE_FRAGMENT_BIT
            , nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        computeCullingDescriptorSetLayout = layoutBuilder.build(engine->getDevice(), VK_SHADER_STAGE_COMPUTE_BIT
            , nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }

    // Compute Cull Pipeline
    {
        VkDescriptorSetLayout layouts[] = {
            bufferAddressesDescriptorSetLayout,
            computeCullingDescriptorSetLayout,
            //engine->get_scene_data_descriptor_set_layout(),
            // TODO: scene data descrtiptor set layout
        };

        VkPipelineLayoutCreateInfo computeCullingPipelineLayoutCreateInfo{};
        computeCullingPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        computeCullingPipelineLayoutCreateInfo.pNext = nullptr;
        computeCullingPipelineLayoutCreateInfo.pSetLayouts = layouts;
        computeCullingPipelineLayoutCreateInfo.setLayoutCount = 3;
        computeCullingPipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
        computeCullingPipelineLayoutCreateInfo.pushConstantRangeCount = 0;


        VK_CHECK(vkCreatePipelineLayout(engine->getDevice(), &computeCullingPipelineLayoutCreateInfo, nullptr, &_computeCullingPipelineLayout));

        VkShaderModule computeShader;
        if (!vk_helpers::loadShaderModule("shaders/gpu_cull.comp.spv", engine->getDevice(), &computeShader)) {
            fmt::print("Error when building the compute shader (gpu_cull.comp.spv)\n"); abort();
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
        computePipelineCreateInfo.layout = _computeCullingPipelineLayout;
        computePipelineCreateInfo.stage = stageinfo;
        computePipelineCreateInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

        VK_CHECK(vkCreateComputePipelines(engine->getDevice(), VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &_computeCullingPipeline));

        vkDestroyShaderModule(engine->getDevice(), computeShader, nullptr);
    }

    staticsInitialized = true;
}
