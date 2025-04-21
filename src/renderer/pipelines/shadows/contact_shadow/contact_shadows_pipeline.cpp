//
// Created by William on 2025-04-14.
//

#include "contact_shadows_pipeline.h"

#include "contact_shadows_pipeline_types.h"
#include "src/renderer/resource_manager.h"
#include "src/core/camera/camera.h"
#include "src/renderer/lighting/directional_light.h"

namespace will_engine::contact_shadows_pipeline
{
ContactShadowsPipeline::ContactShadowsPipeline(ResourceManager& resourceManager) : resourceManager(resourceManager)
{
    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // MRT depth buffer
    layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // ss contact shadows image
    layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // debug image

    descriptorSetLayout = resourceManager.createDescriptorSetLayout(layoutBuilder, VK_SHADER_STAGE_COMPUTE_BIT,
                                                                    VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    VkPushConstantRange pushConstants{};
    pushConstants.offset = 0;
    pushConstants.size = sizeof(ContactShadowsPushConstants);
    pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayout setLayouts[2];
    setLayouts[0] = resourceManager.getSceneDataLayout();
    setLayouts[1] = descriptorSetLayout;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.pSetLayouts = setLayouts;
    layoutInfo.setLayoutCount = 2;
    layoutInfo.pPushConstantRanges = &pushConstants;
    layoutInfo.pushConstantRangeCount = 1;

    pipelineLayout = resourceManager.createPipelineLayout(layoutInfo);
    createPipeline();

    VkSamplerCreateInfo samplerInfo = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    depthSampler = resourceManager.createSampler(samplerInfo);

    VkImageUsageFlags usage{};
    usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VkImageCreateInfo imgInfo = vk_helpers::imageCreateInfo(contactShadowFormat, usage, {RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1});
    contactShadowImage = resourceManager.createImage(imgInfo);

    usage = {};
    usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    imgInfo = vk_helpers::imageCreateInfo(debugFormat, usage, {RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1});
    debugImage = resourceManager.createImage(imgInfo);

    descriptorBufferSampler = resourceManager.createDescriptorBufferSampler(descriptorSetLayout, 1);
}

ContactShadowsPipeline::~ContactShadowsPipeline()
{
    resourceManager.destroyPipeline(pipeline);
    resourceManager.destroyPipelineLayout(pipelineLayout);
    resourceManager.destroyDescriptorSetLayout(descriptorSetLayout);

    resourceManager.destroySampler(depthSampler);

    resourceManager.destroyImage(contactShadowImage);
    resourceManager.destroyImage(debugImage);
    resourceManager.destroyDescriptorBuffer(descriptorBufferSampler);
}

void ContactShadowsPipeline::setupDescriptorBuffer(const VkImageView& depthImageView)
{
    std::vector<DescriptorImageData> imageDescriptors{};
    imageDescriptors.reserve(3);

    imageDescriptors.push_back(
        {
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            {depthSampler, depthImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            false
        });
    imageDescriptors.push_back(
        {
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            {VK_NULL_HANDLE, contactShadowImage.imageView, VK_IMAGE_LAYOUT_GENERAL},
            false
        });
    imageDescriptors.push_back({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        {VK_NULL_HANDLE, debugImage.imageView, VK_IMAGE_LAYOUT_GENERAL},
        false
    });

    resourceManager.setupDescriptorBufferSampler(descriptorBufferSampler, imageDescriptors, 0);
}

void ContactShadowsPipeline::draw(VkCommandBuffer cmd, const ContactShadowsDrawInfo& drawInfo)
{
    vk_helpers::transitionImage(cmd, debugImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::clearColorImage(cmd, VK_IMAGE_ASPECT_COLOR_BIT, contactShadowImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

    ContactShadowsPushConstants push{drawInfo.push};

    const DispatchList dispatchList = buildDispatchList(drawInfo.camera, drawInfo.light);

    push.lightCoordinate = glm::vec4(dispatchList.LightCoordinate_Shader[0], dispatchList.LightCoordinate_Shader[1], dispatchList.LightCoordinate_Shader[2], dispatchList.LightCoordinate_Shader[3]);
    for (int32_t i = 0; i < dispatchList.DispatchCount; ++i) {
        push.waveOffset = glm::ivec2(dispatchList.Dispatch[i].WaveOffset_Shader[0], dispatchList.Dispatch[i].WaveOffset_Shader[1]);

        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ContactShadowsPushConstants), &push);

        VkDescriptorBufferBindingInfoEXT bindingInfos[2] = {};
        bindingInfos[0] = drawInfo.sceneDataBinding;
        bindingInfos[1] = descriptorBufferSampler.getDescriptorBufferBindingInfo();
        vkCmdBindDescriptorBuffersEXT(cmd, 2, bindingInfos);

        constexpr std::array<uint32_t, 2> indices{0, 1};
        const std::array offsets{drawInfo.sceneDataOffset, ZERO_DEVICE_SIZE};

        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 2, indices.data(), offsets.data());

        const int32_t* waveCount = dispatchList.Dispatch[i].WaveCount;
        vkCmdDispatch(cmd, waveCount[0], waveCount[1], waveCount[2]);
    }
}

void ContactShadowsPipeline::createPipeline()
{
    resourceManager.destroyPipeline(pipeline);
    VkShaderModule computeShader = resourceManager.createShaderModule("shaders/shadows/contact_shadow_pass.comp");

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = computeShader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    pipeline = resourceManager.createComputePipeline(pipelineInfo);
    resourceManager.destroyShaderModule(computeShader);
}

DispatchList ContactShadowsPipeline::buildDispatchList(const Camera* camera, const DirectionalLight& mainLight)
{
    DispatchList result = {};

    glm::vec4 lightProjection = camera->getViewProjMatrix() * glm::vec4(-mainLight.getDirection(), 0.0f);

    // Floating point division in the shader has a practical limit for precision when the light is *very* far off screen (~1m pixels+)
    // So when computing the light XY coordinate, use an adjusted w value to handle these extreme values
    float xy_light_w = lightProjection[3];
    const float FP_limit = 0.000002f * static_cast<float>(CONTACT_SHADOW_WAVE_SIZE);

    if (xy_light_w >= 0 && xy_light_w < FP_limit) xy_light_w = FP_limit;
    else if (xy_light_w < 0 && xy_light_w > -FP_limit) xy_light_w = -FP_limit;

    // Need precise XY pixel coordinates of the light
    result.LightCoordinate_Shader[0] = ((lightProjection[0] / xy_light_w) * +0.5f + 0.5f) * RENDER_EXTENT_WIDTH;
    result.LightCoordinate_Shader[1] = ((lightProjection[1] / xy_light_w) * -0.5f + 0.5f) * RENDER_EXTENT_HEIGHT;
    result.LightCoordinate_Shader[2] = lightProjection[3] == 0 ? 0 : (lightProjection[2] / lightProjection[3]);
    result.LightCoordinate_Shader[3] = lightProjection[3] > 0 ? 1 : -1;

    int32_t light_xy[2] = {static_cast<int32_t>(result.LightCoordinate_Shader[0] + 0.5f), static_cast<int32_t>(result.LightCoordinate_Shader[1] + 0.5f)};

    // Make the bounds inclusive, relative to the light
    const int32_t biased_bounds[4] =
    {
        0 - light_xy[0],
        -static_cast<int32_t>(RENDER_EXTENT_HEIGHT - light_xy[1]),
        static_cast<int32_t>(RENDER_EXTENT_WIDTH - light_xy[0]),
        -(0 - light_xy[1]),
    };

    // Process 4 quadrants around the light center,
    // They each form a rectangle with one corner on the light XY coordinate
    // If the rectangle isn't square, it will need breaking in two on the larger axis
    // 0 = bottom left, 1 = bottom right, 2 = top left, 2 = top right
    for (int q = 0; q < 4; q++) {
        // Quads 0 and 3 needs to be +1 vertically, 1 and 2 need to be +1 horizontally
        bool vertical = q == 0 || q == 3;

        // Bounds relative to the quadrant
        const int bounds[4] =
        {
            bend_max(0, ((q & 1) ? biased_bounds[0] : -biased_bounds[2])) / CONTACT_SHADOW_WAVE_SIZE,
            bend_max(0, ((q & 2) ? biased_bounds[1] : -biased_bounds[3])) / CONTACT_SHADOW_WAVE_SIZE,
            bend_max(0, (((q & 1) ? biased_bounds[2] : -biased_bounds[0]) + CONTACT_SHADOW_WAVE_SIZE * (vertical ? 1 : 2) - 1)) / CONTACT_SHADOW_WAVE_SIZE,
            bend_max(0, (((q & 2) ? biased_bounds[3] : -biased_bounds[1]) + CONTACT_SHADOW_WAVE_SIZE * (vertical ? 2 : 1) - 1)) / CONTACT_SHADOW_WAVE_SIZE,
        };

        if ((bounds[2] - bounds[0]) > 0 && (bounds[3] - bounds[1]) > 0) {
            int bias_x = (q == 2 || q == 3) ? 1 : 0;
            int bias_y = (q == 1 || q == 3) ? 1 : 0;

            DispatchData& disp = result.Dispatch[result.DispatchCount++];

            disp.WaveCount[0] = CONTACT_SHADOW_WAVE_SIZE;
            disp.WaveCount[1] = bounds[2] - bounds[0];
            disp.WaveCount[2] = bounds[3] - bounds[1];
            disp.WaveOffset_Shader[0] = ((q & 1) ? bounds[0] : -bounds[2]) + bias_x;
            disp.WaveOffset_Shader[1] = ((q & 2) ? -bounds[3] : bounds[1]) + bias_y;

            // We want the far corner of this quadrant relative to the light,
            // as we need to know where the diagonal light ray intersects with the edge of the bounds
            int axis_delta = +biased_bounds[0] - biased_bounds[1];
            if (q == 1) axis_delta = +biased_bounds[2] + biased_bounds[1];
            if (q == 2) axis_delta = -biased_bounds[0] - biased_bounds[3];
            if (q == 3) axis_delta = -biased_bounds[2] + biased_bounds[3];

            axis_delta = (axis_delta + CONTACT_SHADOW_WAVE_SIZE - 1) / CONTACT_SHADOW_WAVE_SIZE;

            if (axis_delta > 0) {
                DispatchData& disp2 = result.Dispatch[result.DispatchCount++];

                // Take copy of current volume
                disp2 = disp;

                if (q == 0) {
                    // Split on Y, split becomes -1 larger on x
                    disp2.WaveCount[2] = bend_min(disp.WaveCount[2], axis_delta);
                    disp.WaveCount[2] -= disp2.WaveCount[2];
                    disp2.WaveOffset_Shader[1] = disp.WaveOffset_Shader[1] + disp.WaveCount[2];
                    disp2.WaveOffset_Shader[0]--;
                    disp2.WaveCount[1]++;
                }
                if (q == 1) {
                    // Split on X, split becomes +1 larger on y
                    disp2.WaveCount[1] = bend_min(disp.WaveCount[1], axis_delta);
                    disp.WaveCount[1] -= disp2.WaveCount[1];
                    disp2.WaveOffset_Shader[0] = disp.WaveOffset_Shader[0] + disp.WaveCount[1];
                    disp2.WaveCount[2]++;
                }
                if (q == 2) {
                    // Split on X, split becomes -1 larger on y
                    disp2.WaveCount[1] = bend_min(disp.WaveCount[1], axis_delta);
                    disp.WaveCount[1] -= disp2.WaveCount[1];
                    disp.WaveOffset_Shader[0] += disp2.WaveCount[1];
                    disp2.WaveCount[2]++;
                    disp2.WaveOffset_Shader[1]--;
                }
                if (q == 3) {
                    // Split on Y, split becomes +1 larger on x
                    disp2.WaveCount[2] = bend_min(disp.WaveCount[2], axis_delta);
                    disp.WaveCount[2] -= disp2.WaveCount[2];
                    disp.WaveOffset_Shader[1] += disp2.WaveCount[2];
                    disp2.WaveCount[1]++;
                }

                // Remove if too small
                if (disp2.WaveCount[1] <= 0 || disp2.WaveCount[2] <= 0) {
                    disp2 = result.Dispatch[--result.DispatchCount];
                }
                if (disp.WaveCount[1] <= 0 || disp.WaveCount[2] <= 0) {
                    disp = result.Dispatch[--result.DispatchCount];
                }
            }
        }
    }

    // Scale the shader values by the wave count, the shader expects this
    for (int i = 0; i < result.DispatchCount; i++) {
        result.Dispatch[i].WaveOffset_Shader[0] *= CONTACT_SHADOW_WAVE_SIZE;
        result.Dispatch[i].WaveOffset_Shader[1] *= CONTACT_SHADOW_WAVE_SIZE;
    }

    return result;
}
}
