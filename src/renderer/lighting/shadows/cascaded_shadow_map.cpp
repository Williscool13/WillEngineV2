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
        pipelineBuilder.enableDepthBias(-1.25f, 0, -1.75f); // negateive for reversed depth buffer
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
        vk_helpers::transitionImage(cmd, shadowMaps->depthShadowMap.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

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

        vk_helpers::transitionImage(cmd, shadowMaps->depthShadowMap.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
    }


    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void CascadedShadowMap::getFrustumCornersWorldSpace(const CameraProperties& cameraProperties, float nearPlane, float farPlane, glm::vec4 corners[8])
{
    const glm::vec3 center = cameraProperties.position;
    const glm::vec3 view_dir = cameraProperties.forward;
    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    const glm::vec3 right = glm::normalize(glm::cross(view_dir, up));
    const glm::vec3 up_corrected = glm::normalize(glm::cross(right, view_dir));

    // Calculate near/far heights and widths using FOV
    const float near_height = glm::tan(cameraProperties.fov * 0.5f) * nearPlane;
    const float near_width = near_height * cameraProperties.aspect;
    const float far_height = glm::tan(cameraProperties.fov * 0.5f) * farPlane;
    const float far_width = far_height * cameraProperties.aspect;

    // Near face corners
    const glm::vec3 near_center = center + view_dir * nearPlane;
    corners[0] = glm::vec4(near_center - up_corrected * near_height - right * near_width, 1.0f);  // bottom-left
    corners[1] = glm::vec4(near_center + up_corrected * near_height - right * near_width, 1.0f);  // top-left
    corners[2] = glm::vec4(near_center + up_corrected * near_height + right * near_width, 1.0f);  // top-right
    corners[3] = glm::vec4(near_center - up_corrected * near_height + right * near_width, 1.0f);  // bottom-right

    // Far face corners
    const glm::vec3 far_center = center + view_dir * farPlane;
    corners[4] = glm::vec4(far_center - up_corrected * far_height - right * far_width, 1.0f);    // bottom-left
    corners[5] = glm::vec4(far_center + up_corrected * far_height - right * far_width, 1.0f);    // top-left
    corners[6] = glm::vec4(far_center + up_corrected * far_height + right * far_width, 1.0f);    // top-right
    corners[7] = glm::vec4(far_center - up_corrected * far_height + right * far_width, 1.0f);    // bottom-right
}

glm::mat4 CascadedShadowMap::getLightSpaceMatrix(const glm::vec3 directionalLightDirection, const CameraProperties& cameraProperties, const float cascadeNear, const float cascadeFar)
{
    constexpr int32_t numberOfCorners = 8;
    glm::vec4 corners[numberOfCorners];
    getFrustumCornersWorldSpace(cameraProperties, cascadeNear, cascadeFar, corners);

    auto center = glm::vec3(0, 0, 0);
    for (const auto& v : corners) {
        center += glm::vec3(v);
    }
    center /= numberOfCorners;

    static int index{0};
    fmt::print("Center {}: {}, {}, {}\n", index % 5, center.x, center.y, center.z);
    index++;

    const glm::mat4 lightView = glm::lookAt(center + directionalLightDirection, center, glm::vec3(0.0f, 1.0f, 0.0f));

    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();
    for (const auto& v : corners) {
        const auto trf = lightView * v;
        minX = std::min(minX, trf.x);
        maxX = std::max(maxX, trf.x);
        minY = std::min(minY, trf.y);
        maxY = std::max(maxY, trf.y);
        minZ = std::min(minZ, trf.z);
        maxZ = std::max(maxZ, trf.z);
    }

    // Tune this parameter according to the scene
    constexpr float zMult = 10.0f;
    if (minZ < 0) {
        minZ *= zMult;
    } else {
        minZ /= zMult;
    }
    if (maxZ < 0) {
        maxZ /= zMult;
    } else {
        maxZ *= zMult;
    }

    const glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
    return lightProjection * lightView;
}


void CascadedShadowMap::getLightSpaceMatrices(const glm::vec3 directionalLightDirection, const CameraProperties& cameraProperties, glm::mat4 matrices[SHADOW_MAP_COUNT])
{
    for (size_t i = 0; i < SHADOW_MAP_COUNT; ++i) {
        if (i == 0) {
            matrices[i] = getLightSpaceMatrix(directionalLightDirection, cameraProperties, cameraProperties.nearPlane, normalizedCascadeLevels[i] * cameraProperties.nearPlane);
        } else if (i < SHADOW_CASCADE_COUNT) {
            matrices[i] = getLightSpaceMatrix(directionalLightDirection, cameraProperties, normalizedCascadeLevels[i - 1] * cameraProperties.nearPlane,
                                              normalizedCascadeLevels[i] * cameraProperties.nearPlane);
        } else {
            matrices[i] = getLightSpaceMatrix(directionalLightDirection, cameraProperties, normalizedCascadeLevels[i - 1] * cameraProperties.nearPlane, cameraProperties.farPlane);
        }
    }
}
