//
// Created by William on 2025-01-01.
//

#include "cascaded_shadow_map.h"

#include "cascaded_shadow_map_descriptor_layouts.h"
#include "shadow_types.h"
#include "src/core/camera/camera.h"
#include "src/renderer/resource_manager.h"
#include "src/renderer/vk_pipelines.h"
#include "src/renderer/lighting/directional_light.h"
#include "src/renderer/render_object/render_object.h"
#include "src/util/render_utils.h"

#include "fmt/format.h"

CascadedShadowMap::CascadedShadowMap(const VulkanContext& context, ResourceManager& resourceManager)
    : context(context), resourceManager(resourceManager)
{}

CascadedShadowMap::~CascadedShadowMap()
{
    for (CascadeShadowMapData& cascadeShadowMapData : shadowMaps) {
        resourceManager.destroyImage(cascadeShadowMapData.depthShadowMap);
        cascadeShadowMapData.depthShadowMap = {};
    }

    resourceManager.destroyBuffer(cascadedShadowMapData);

    cascadedShadowMapDescriptorBufferSampler.destroy(context.allocator);
    cascadedShadowMapDescriptorBufferUniform.destroy(context.allocator);

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
    generateSplits(cascadeNear, cascadeFar);

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

        for (CascadeShadowMapData& cascadeShadowMapData : shadowMaps) {
            cascadeShadowMapData.depthShadowMap = resourceManager.createImage({extent.width, extent.height, 1}, depthFormat,
                                                                              VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        }
    }

    // Pipeline Layout
    {
        VkDescriptorSetLayout layouts[2];
        layouts[0] = shadowMapPipelineCreateInfo.cascadedShadowMapUniformLayout;
        layouts[1] = shadowMapPipelineCreateInfo.modelAddressesLayout;

        VkPushConstantRange pushConstantRange;
        pushConstantRange.size = sizeof(CascadedShadowMapGenerationPushConstants);
        pushConstantRange.offset = 0;
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkPipelineLayoutCreateInfo layoutInfo = vk_helpers::pipelineLayoutCreateInfo();
        layoutInfo.pNext = nullptr;
        layoutInfo.setLayoutCount = 2;
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
        pipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
        // set later during shadow pass
        pipelineBuilder.enableDepthBias(0.0f, 0, 0.0f);
        pipelineBuilder.disableMultisampling();
        pipelineBuilder.setupBlending(PipelineBuilder::BlendMode::NO_BLEND);
        //pipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
        pipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
        pipelineBuilder.setupRenderer({}, depthFormat);
        pipelineBuilder.setupPipelineLayout(pipelineLayout);

        pipeline = pipelineBuilder.buildPipeline(context.device, VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT, {VK_DYNAMIC_STATE_DEPTH_BIAS});

        vkDestroyShaderModule(context.device, vertShader, nullptr);
        vkDestroyShaderModule(context.device, fragShader, nullptr);
    }

    // Create Descriptor Buffer
    {
        std::vector<DescriptorImageData> textureDescriptors;
        for (const CascadeShadowMapData& shadowMapData : shadowMaps) {
            const VkDescriptorImageInfo imageInfo{.sampler = sampler, .imageView = shadowMapData.depthShadowMap.imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            textureDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfo, false});
        }
        cascadedShadowMapDescriptorBufferSampler = DescriptorBufferSampler(context, shadowMapPipelineCreateInfo.cascadedShadowMapSamplerLayout, 1);
        cascadedShadowMapDescriptorBufferSampler.setupData(context.device, textureDescriptors);


        cascadedShadowMapData = resourceManager.createBuffer(sizeof(CascadeShadowData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        cascadedShadowMapDescriptorBufferUniform = DescriptorBufferUniform(context, shadowMapPipelineCreateInfo.cascadedShadowMapUniformLayout, 1);
        std::vector<DescriptorUniformData> cascadeData{1};
        cascadeData[0] = DescriptorUniformData{.uniformBuffer = cascadedShadowMapData, .allocSize = sizeof(CascadeShadowData)};
        cascadedShadowMapDescriptorBufferUniform.setupData(context.device, cascadeData);

        const auto data = static_cast<CascadeShadowData*>(cascadedShadowMapData.info.pMappedData);
        data->nearShadowPlane = cascadeNear;
        data->farShadowPlane = cascadeFar;
    }
}

void CascadedShadowMap::updateCascadeData(VkCommandBuffer cmd, const DirectionalLight& mainLight, const CameraProperties& cameraProperties)
{
    if (cascadedShadowMapData.buffer == VK_NULL_HANDLE) { return; }

    // doesn't need to happen every frame, only if near/far changes
    // generateSplits(cameraProperties.nearPlane, cameraProperties.farPlane);

    for (CascadeShadowMapData& shadowData : shadowMaps) {
        shadowData.lightViewProj = getLightSpaceMatrix(mainLight.getDirection(), cameraProperties, shadowData.split.nearPlane, shadowData.split.farPlane);
    }

    const auto data = static_cast<CascadeShadowData*>(cascadedShadowMapData.info.pMappedData);
    for (size_t i = 0; i < SHADOW_CASCADE_COUNT; i++) {
        data->cascadeSplits[i] = shadowMaps[i].split;
        data->lightViewProj[i] = shadowMaps[i].lightViewProj;
    }
    data->directionalLightData = mainLight.getData();
}

void CascadedShadowMap::draw(VkCommandBuffer cmd, const CascadedShadowMapDrawInfo& drawInfo) const
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Shadow Pass";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    constexpr VkClearValue clearValue = {1.0f, 1.0f};

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

        vkCmdSetDepthBias(cmd, cascadeBias[cascadeShadowMapData.cascadeLevel][0], 0.0f, cascadeBias[cascadeShadowMapData.cascadeLevel][1]);

        CascadedShadowMapGenerationPushConstants pushConstants{};
        pushConstants.cascadeIndex = cascadeShadowMapData.cascadeLevel;
        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(CascadedShadowMapGenerationPushConstants), &pushConstants);

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

            VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[2];
            constexpr uint32_t shadowDataIndex{0};
            constexpr uint32_t addressIndex{1};
            descriptorBufferBindingInfo[0] = cascadedShadowMapDescriptorBufferUniform.getDescriptorBufferBindingInfo();
            descriptorBufferBindingInfo[1] = renderObject->getAddressesDescriptorBuffer().getDescriptorBufferBindingInfo();

            vkCmdBindDescriptorBuffersEXT(cmd, 2, descriptorBufferBindingInfo);

            const VkDeviceSize shadowDataOffset{cascadedShadowMapDescriptorBufferUniform.getDescriptorBufferSize() * 0};
            const VkDeviceSize addressOffset{renderObject->getAddressesDescriptorBuffer().getDescriptorBufferSize() * 0};
            vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &shadowDataIndex, &shadowDataOffset);
            vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &addressIndex, &addressOffset);


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

    glm::vec3 center{0.0f};
    for (const auto& v : corners) {
        center += glm::vec3(v);
    }
    center /= numberOfCorners;
    const auto lightView = glm::lookAt(
        center - lightDirection,
        center,
        //cameraProperties.up
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
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
    constexpr float zMult = 5.0f;
    minZ *= zMult;
    maxZ *= zMult;

    const glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
    return lightProjection * lightView;
}

void CascadedShadowMap::generateSplits(float nearPlane, float farPlane)
{
    // https://github.com/diharaw/cascaded-shadow-maps
    // Practical Split Scheme: https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html

    // need to reverse the reversed depth buffer for correct frustum calculations
    // matrix calculations use these values as "distance from camera". Reversed depth buffer just doesn't jive.
    /*const auto temp = nearPlane;
    nearPlane = farPlane;
    farPlane = temp;*/

    const float ratio = farPlane / nearPlane;
    shadowMaps[0].split.nearPlane = nearPlane;

    for (size_t i = 1; i < SHADOW_CASCADE_COUNT; i++) {
        const float si = static_cast<float>(i) / static_cast<float>(SHADOW_CASCADE_COUNT);

        const float uniformTerm = nearPlane + (farPlane - nearPlane) * si;
        const float logTerm = nearPlane * std::pow(ratio, si);
        const float nearValue = LAMBDA * logTerm + (1.0f - LAMBDA) * uniformTerm;

        const float farValue = nearValue * OVERLAP;

        shadowMaps[i].split.nearPlane = nearValue;
        shadowMaps[i - 1].split.farPlane = farValue;
    }

    shadowMaps[SHADOW_CASCADE_COUNT - 1].split.farPlane = farPlane;
}
