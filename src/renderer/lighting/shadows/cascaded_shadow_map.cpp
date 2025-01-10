//
// Created by William on 2025-01-01.
//

#include "cascaded_shadow_map.h"

#include "shadow_map_descriptor_layouts.h"
#include "shadow_types.h"
#include "glm/detail/_noise.hpp"
#include "glm/detail/_noise.hpp"
#include "src/core/camera/camera.h"
#include "src/renderer/resource_manager.h"
#include "src/renderer/vk_pipelines.h"
#include "src/renderer/lighting/directional_light.h"
#include "src/renderer/render_object/render_object.h"
#include "src/util/render_utils.h"

CascadedShadowMap::CascadedShadowMap(const VulkanContext& context, ResourceManager& resourceManager)
    : context(context), resourceManager(resourceManager)
{}

CascadedShadowMap::~CascadedShadowMap()
{
    for (CascadeShadowMapData& cascadeShadowMapData : shadowMaps) {
        resourceManager.destroyImage(cascadeShadowMapData.depthShadowMap);
        cascadeShadowMapData.depthShadowMap = {};
    }

    shadowMapDescriptorBuffer.destroy(context.allocator);

    if (sampler) {
        vkDestroySampler(context.device, sampler, nullptr);
        sampler = VK_NULL_HANDLE;
    }


    if (pipeline) {
        vkDestroyPipeline(context.device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
    if (pipelineLayout) {
        vkDestroyPipelineLayout(context.device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }
}

void CascadedShadowMap::init(const ShadowMapPipelineCreateInfo& shadowMapPipelineCreateInfo)
{
    // Create Resources
    {
        VkSamplerCreateInfo samplerCreateInfo = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCreateInfo.minLod = 0;
        samplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;
        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;

        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        samplerCreateInfo.compareEnable = VK_TRUE;
        samplerCreateInfo.compareOp = VK_COMPARE_OP_LESS;

        //????????
        //sampl.anisotropyEnable = VK_TRUE;
        //sampl.maxAnisotropy = 16.0f;

        vkCreateSampler(context.device, &samplerCreateInfo, nullptr, &sampler);

        //glm::ortho()
        for (CascadeShadowMapData& cascadeShadowMapData : shadowMaps) {
            cascadeShadowMapData.depthShadowMap = resourceManager.createImage({extent.width, extent.height, 1}, depthFormat,
                                                                              VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        }
    }

    // Pipeline Layout
    {
        VkDescriptorSetLayout layouts[1];
        layouts[0] = shadowMapPipelineCreateInfo.modelAddressesLayout;

        VkPushConstantRange pushConstantRange;
        pushConstantRange.size = sizeof(ShadowMapPushConstants);
        pushConstantRange.offset = 0;
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkPipelineLayoutCreateInfo layoutInfo = vk_helpers::pipelineLayoutCreateInfo();
        layoutInfo.pNext = nullptr;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = layouts;
        layoutInfo.pPushConstantRanges = &pushConstantRange;
        layoutInfo.pushConstantRangeCount = 1;

        VK_CHECK(vkCreatePipelineLayout(context.device, &layoutInfo, nullptr, &pipelineLayout));
    }

    // Create Pipelines
    {
        VkShaderModule vertShader;
        if (!vk_helpers::loadShaderModule("shaders/shadows/shadow_pass.vert.spv", context.device, &vertShader)) {
            throw std::runtime_error("Error when building vertex shader (shadow_pass.vert.spv)");
        }

        VkShaderModule fragShader;
        if (!vk_helpers::loadShaderModule("shaders/shadows/shadow_pass.frag.spv", context.device, &fragShader)) {
            throw std::runtime_error("Error when building fragment shader (shadow_pass.frag.spv)");
        }

        PipelineBuilder pipelineBuilder;
        VkVertexInputBindingDescription mainBinding{};
        mainBinding.binding = 0;
        mainBinding.stride = sizeof(Vertex);
        mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        VkVertexInputAttributeDescription vertexAttributes[1];
        vertexAttributes[0].binding = 0;
        vertexAttributes[0].location = 0;
        vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        vertexAttributes[0].offset = offsetof(Vertex, position);

        pipelineBuilder.setupVertexInput(&mainBinding, 1, vertexAttributes, 1);

        pipelineBuilder.setShaders(vertShader, fragShader);
        pipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        pipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
        //pipelineBuilder.enableDepthBias(-1.25f, 0, -1.75f); // negateive for reversed depth buffer
        pipelineBuilder.disableMultisampling();
        pipelineBuilder.setupBlending(PipelineBuilder::BlendMode::NO_BLEND);
        pipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
        pipelineBuilder.setupRenderer({}, depthFormat);
        pipelineBuilder.setupPipelineLayout(pipelineLayout);

        pipeline = pipelineBuilder.buildPipeline(context.device, VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

        vkDestroyShaderModule(context.device, vertShader, nullptr);
        vkDestroyShaderModule(context.device, fragShader, nullptr);
    }

    // Create Descriptor Buffer
    {
        std::vector<DescriptorImageData> textureDescriptors;
        for (const CascadeShadowMapData& shadowMapData : shadowMaps) {
            const VkDescriptorImageInfo imageInfo{.imageView = shadowMapData.depthShadowMap.imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            textureDescriptors.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfo, false});
        }
        shadowMapDescriptorBuffer = DescriptorBufferSampler(context, shadowMapPipelineCreateInfo.shadowMapLayout, 1);
        shadowMapDescriptorBuffer.setupData(context.device, textureDescriptors);
    }
}

void CascadedShadowMap::draw(VkCommandBuffer cmd, const CascadedShadowMapDrawInfo& drawInfo)
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Shadow Pass";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    constexpr VkClearValue clearValue = {0.0f, 0.0f};

    glm::mat4 lightSpaceMatrices[SHADOW_MAP_COUNT];
    getLightSpaceMatrices(drawInfo.directionalLight.getDirection(), drawInfo.cameraProperties, lightSpaceMatrices);

    for (const CascadeShadowMapData& cascadeShadowMapData : shadowMaps) {
        vk_helpers::transitionImage(cmd, cascadeShadowMapData.depthShadowMap.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
        VkRenderingAttachmentInfo depthAttachment = vk_helpers::attachmentInfo(cascadeShadowMapData.depthShadowMap.imageView, &clearValue, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

        VkRenderingInfo renderInfo{};
        renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderInfo.pNext = nullptr;

        renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, extent};
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = 0;
        renderInfo.pColorAttachments = nullptr;
        renderInfo.pDepthAttachment = &depthAttachment;
        renderInfo.pStencilAttachment = nullptr;

        vkCmdBeginRendering(cmd, &renderInfo);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);


        ShadowMapPushConstants pushConstants{};
        assert(cascadeShadowMapData.cascadeLevel < SHADOW_MAP_COUNT);
        pushConstants.lightMatrix = lightSpaceMatrices[cascadeShadowMapData.cascadeLevel];
        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowMapPushConstants), &pushConstants);

        //  Viewport
        VkViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = extent.width;
        viewport.height = extent.height;
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        //  Scissor
        VkRect2D scissor = {};
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = extent.width;
        scissor.extent.height = extent.height;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        constexpr VkDeviceSize zeroOffset{0};

        for (const RenderObject* renderObject : drawInfo.renderObjects) {
            if (!renderObject->canDraw()) { continue; }

            VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[1];
            constexpr uint32_t addressIndex{0};
            descriptorBufferBindingInfo[0] = renderObject->getAddressesDescriptorBuffer().getDescriptorBufferBindingInfo();

            vkCmdBindDescriptorBuffersEXT(cmd, 1, descriptorBufferBindingInfo);

            const VkDeviceSize addressOffset{drawInfo.currentFrameOverlap * renderObject->getAddressesDescriptorBuffer().getDescriptorBufferSize()};
            vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &addressIndex, &addressOffset);


            vkCmdBindVertexBuffers(cmd, 0, 1, &renderObject->getVertexBuffer().buffer, &zeroOffset);
            vkCmdBindIndexBuffer(cmd, renderObject->getIndexBuffer().buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexedIndirect(cmd, renderObject->getIndirectBuffer().buffer, 0, renderObject->getDrawIndirectCommandCount(), sizeof(VkDrawIndexedIndirectCommand));
        }

        vkCmdEndRendering(cmd);

        vk_helpers::transitionImage(cmd, cascadeShadowMapData.depthShadowMap.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
    }


    vkCmdEndDebugUtilsLabelEXT(cmd);
}

glm::mat4 CascadedShadowMap::getLightSpaceMatrix(const glm::vec3 lightDirection, const CameraProperties& cameraProperties, float cascadeNear, float cascadeFar)
{
    constexpr int32_t numberOfCorners = 8;
    glm::vec4 corners[numberOfCorners];
    render_utils::getPerspectiveFrustumCornersWorldSpace(cascadeNear, cascadeFar, cameraProperties.fov, cameraProperties.aspect, cameraProperties.position, cameraProperties.forward, corners);

    glm::vec3 center = cameraProperties.position + cameraProperties.forward * 50.0f;
    glm::vec3 lightPos = center - lightDirection * ((cascadeFar - cascadeNear) / 2.0f);
    const glm::mat4 lightView = glm::lookAt(lightPos, center, glm::vec3{0.0f, 1.0f, 0.0f});

    glm::vec3 tmax{-INFINITY};
    glm::vec3 tmin{INFINITY};

    glm::vec4 transformedCorner0 = lightView * corners[0];
    tmax.z = transformedCorner0.z;
    tmin.z = transformedCorner0.z;
    for (int i = 1; i < numberOfCorners; i++) {
        glm::vec4 transformedCorner = lightView * corners[i];
        if (transformedCorner.z > tmax.z) { tmax.z = transformedCorner.z; }
        if (transformedCorner.z < tmin.z) { tmin.z = transformedCorner.z; }
    }


    const glm::mat4 ortho = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -tmin.z, -tmax.z);
    const glm::mat4 shadowMVP = ortho * lightView;

    for (const auto& corner : corners) {
        glm::vec4 trf = shadowMVP * corner;
        trf.x /= trf.w;
        trf.y /= trf.w;

        if (trf.x > tmax.x) { tmax.x = trf.x; }
        if (trf.x < tmin.x) { tmin.x = trf.x; }
        if (trf.y > tmax.y) { tmax.y = trf.y; }
        if (trf.y < tmin.y) { tmin.y = trf.y; }
    }

    // Compute crop matrix
    glm::vec2 scale(2.0f / (tmax.x - tmin.x), 2.0f / (tmax.y - tmin.y));
    glm::vec2 offset(-0.5f * (tmax.x + tmin.x) * scale.x, -0.5f * (tmax.y + tmin.y) * scale.y);

    glm::mat4 cropMatrix(1.0f);
    cropMatrix[0][0] = scale.x;
    cropMatrix[1][1] = scale.y;
    cropMatrix[0][3] = offset.x;
    cropMatrix[1][3] = offset.y;
    cropMatrix = glm::transpose(cropMatrix);

    return cropMatrix * ortho * lightView;
}


void CascadedShadowMap::getLightSpaceMatrices(const glm::vec3 directionalLightDirection, const CameraProperties& cameraProperties, glm::mat4 matrices[SHADOW_MAP_COUNT])
{
    // Generate splits
    // https://github.com/diharaw/cascaded-shadow-maps
    // Practical Split Scheme: https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
    // {
    //     float nd = camera->m_near;
    //     float fd = camera->m_far;
    //
    //     float lambda = m_lambda;
    //     float ratio = fd / nd;
    //     m_splits[0].near_plane = nd;
    //
    //     for (int i = 1; i < m_split_count; i++) {
    //         float si = i / (float) m_split_count;
    //
    //         // Practical Split Scheme: https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
    //         float t_near = lambda * (nd * powf(ratio, si)) + (1 - lambda) * (nd + (fd - nd) * si);
    //         float t_far = t_near * 1.005f;
    //         m_splits[i].near_plane = t_near;
    //         m_splits[i - 1].far_plane = t_far;
    //     }
    //
    //     m_splits[m_split_count - 1].far_plane = fd;
    // }


    assert(USING_REVERSED_DEPTH_BUFFER);
    // need to reverse the reversed depth buffer for correct frustum calculations
    // matrix calculations use these values as "distance from camera". Reversed depth buffer just doesn't jive.
    const float nearPlane = cameraProperties.farPlane;
    const float farPlane = cameraProperties.nearPlane;

    for (size_t i = 0; i < SHADOW_MAP_COUNT; ++i) {
        if (i == 0) {
            matrices[i] = getLightSpaceMatrix(directionalLightDirection, cameraProperties, nearPlane, normalizedCascadeLevels[i] * farPlane);
        } else if (i < SHADOW_CASCADE_COUNT) {
            matrices[i] = getLightSpaceMatrix(directionalLightDirection, cameraProperties, normalizedCascadeLevels[i - 1] * farPlane, normalizedCascadeLevels[i] * farPlane);
        } else {
            matrices[i] = getLightSpaceMatrix(directionalLightDirection, cameraProperties, normalizedCascadeLevels[i - 1] * farPlane, farPlane);
        }
    }
}
