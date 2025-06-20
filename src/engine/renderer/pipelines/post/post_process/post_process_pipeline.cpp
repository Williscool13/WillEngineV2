//
// Created by William on 2025-01-25.
//

#include "post_process_pipeline.h"

#include <array>
#include <fmt/format.h>

#include "engine/renderer/renderer_constants.h"
#include "engine/renderer/resource_manager.h"
#include "engine/renderer/vk_descriptors.h"
#include "engine/renderer/resources/pipeline.h"
#include "engine/renderer/resources/pipeline_layout.h"
#include "engine/renderer/resources/shader_module.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_sampler.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_types.h"

namespace will_engine::renderer
{
PostProcessPipeline::PostProcessPipeline(ResourceManager& resourceManager)
    : resourceManager(resourceManager)
{
    DescriptorLayoutBuilder layoutBuilder{2};
    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // taa resolve image
    layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // post process result
    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = layoutBuilder.build(
        VK_SHADER_STAGE_COMPUTE_BIT,
        VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
    );

    descriptorSetLayout = resourceManager.createResource<DescriptorSetLayout>(layoutCreateInfo);

    VkPushConstantRange pushConstants{};
    pushConstants.offset = 0;
    pushConstants.size = sizeof(PostProcessPushConstants);
    pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    const std::array setLayouts{
        resourceManager.getSceneDataLayout(),
        descriptorSetLayout->layout,
    };


    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.pSetLayouts = setLayouts.data();
    layoutInfo.setLayoutCount = setLayouts.size();
    layoutInfo.pPushConstantRanges = &pushConstants;
    layoutInfo.pushConstantRangeCount = 1;

    pipelineLayout = resourceManager.createResource<PipelineLayout>(layoutInfo);

    createPipeline();

    descriptorBuffer = resourceManager.createResource<DescriptorBufferSampler>(descriptorSetLayout->layout, 1);
}

PostProcessPipeline::~PostProcessPipeline()
{
    resourceManager.destroyResource(std::move(pipeline));
    resourceManager.destroyResource(std::move(pipelineLayout));
    resourceManager.destroyResource(std::move(descriptorSetLayout));
    resourceManager.destroyResource(std::move(descriptorBuffer));
}

void PostProcessPipeline::setupDescriptorBuffer(const PostProcessDescriptor& bufferInfo)
{
    VkDescriptorImageInfo inputImage{};
    inputImage.sampler = bufferInfo.sampler;
    inputImage.imageView = bufferInfo.inputImage;
    inputImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo outputImage{};
    outputImage.imageView = bufferInfo.outputImage;
    outputImage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    std::array<DescriptorImageData, 2> descriptors{
        DescriptorImageData{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, inputImage, false},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, outputImage, false},
    };

    descriptorBuffer->setupData(descriptors, 0);
}

void PostProcessPipeline::draw(VkCommandBuffer cmd, PostProcessDrawInfo drawInfo) const
{
    if (!descriptorBuffer) {
        fmt::print("Descriptor buffer not yet set up");
        return;
    }

    VkDebugUtilsLabelEXT label{};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Post Process Pass";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline);

    PostProcessPushConstants push{};
    push.postProcessFlags = static_cast<uint32_t>(drawInfo.postProcessFlags);

    vkCmdPushConstants(cmd, pipelineLayout->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PostProcessPushConstants), &push);

    const std::array bindingInfos{
        drawInfo.sceneDataBinding,
        descriptorBuffer->getBindingInfo()
    };
    vkCmdBindDescriptorBuffersEXT(cmd, 2, bindingInfos.data());

    constexpr std::array<uint32_t, 2> indices{0, 1};
    const std::array<VkDeviceSize, 2> offsets{drawInfo.sceneDataOffset, 0};
    vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout->layout, 0, 2, indices.data(), offsets.data());

    const auto x = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_WIDTH / 16.0f));
    const auto y = static_cast<uint32_t>(std::ceil(RENDER_EXTENT_HEIGHT / 16.0f));
    vkCmdDispatch(cmd, x, y, 1);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void PostProcessPipeline::createPipeline()
{
    resourceManager.destroyResource(std::move(pipeline));
    ShaderModulePtr shader = resourceManager.createResource<ShaderModule>("shaders/postProcess.comp");

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = shader->shader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.layout = pipelineLayout->layout;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    pipeline = resourceManager.createResource<Pipeline>(pipelineInfo);
}
}
