//
// Created by William on 2025-01-26.
//

#include "cascaded_shadow_map.h"

#include <ranges>

#include "volk/volk.h"
#include "engine/core/camera/camera.h"
#include "engine/core/game_object/terrain.h"
#include "engine/renderer/renderer_constants.h"
#include "engine/renderer/assets/render_object/render_object.h"
#include "engine/renderer/assets/render_object/render_object_types.h"
#include "engine/renderer/terrain/terrain_chunk.h"
#include "engine/util/math_constants.h"
#include "engine/util/render_utils.h"

namespace will_engine::renderer
{
CascadedShadowMap::CascadedShadowMap(ResourceManager& resourceManager)
    : resourceManager(resourceManager)
{
    // (Uniform) Shadow Map Layout - Used by deferred resolve
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        cascadedShadowMapUniformLayout = resourceManager.createDescriptorSetLayout(
            layoutBuilder,
            static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }

    // (Sampler) Shadow Map Layout - Used by deferred resolve
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SHADOW_CASCADE_COUNT);
        cascadedShadowMapSamplerLayout = resourceManager.createDescriptorSetLayout(
            layoutBuilder,
            static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }

    generateSplits();

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

    samplerCreateInfo.anisotropyEnable = VK_TRUE;
    samplerCreateInfo.maxAnisotropy = 16.0f;

    sampler = resourceManager.createSampler(samplerCreateInfo);

    for (int32_t i = 0; i < SHADOW_CASCADE_COUNT; ++i) {
        CascadeShadowMapData& cascadeShadowMapData = shadowMaps[i];
        VkImageUsageFlags usage{};
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        VkImageCreateInfo imgInfo = vk_helpers::imageCreateInfo(CASCADE_DEPTH_FORMAT, usage, csmProperties.cascadeExtents[i]);
        cascadeShadowMapData.depthShadowMap = resourceManager.createImage(imgInfo);
    }

    //
    {
        VkDescriptorSetLayout layouts[2];
        layouts[0] = cascadedShadowMapUniformLayout.layout;
        layouts[1] = resourceManager.getRenderObjectAddressesLayout();

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

        renderObjectPipelineLayout = resourceManager.createPipelineLayout(layoutInfo);
    }

    createRenderObjectPipeline();
    //
    {
        VkDescriptorSetLayout layouts[1];
        layouts[0] = cascadedShadowMapUniformLayout.layout;

        VkPushConstantRange pushConstantRange;
        pushConstantRange.size = sizeof(CascadedShadowMapGenerationPushConstants);
        pushConstantRange.offset = 0;
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;

        VkPipelineLayoutCreateInfo layoutInfo = vk_helpers::pipelineLayoutCreateInfo();
        layoutInfo.pNext = nullptr;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = layouts;
        layoutInfo.pPushConstantRanges = &pushConstantRange;
        layoutInfo.pushConstantRangeCount = 1;
        terrainPipelineLayout = resourceManager.createPipelineLayout(layoutInfo);
    }
    createTerrainPipeline();


    std::vector<DescriptorImageData> textureDescriptors;
    for (const CascadeShadowMapData& shadowMapData : shadowMaps) {
        const VkDescriptorImageInfo imageInfo{
            .sampler = sampler.sampler, .imageView = shadowMapData.depthShadowMap.imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        textureDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfo, false});
    }
    cascadedShadowMapDescriptorBufferSampler = resourceManager.createDescriptorBufferSampler(cascadedShadowMapSamplerLayout.layout, 1);
    resourceManager.setupDescriptorBufferSampler(cascadedShadowMapDescriptorBufferSampler, textureDescriptors, 0);


    cascadedShadowMapDescriptorBufferUniform = resourceManager.createDescriptorBufferUniform(cascadedShadowMapUniformLayout.layout, FRAME_OVERLAP);
    for (int32_t i = 0; i < FRAME_OVERLAP; ++i) {
        cascadedShadowMapDatas[i] = resourceManager.createHostSequentialBuffer(sizeof(CascadeShadowData));
        std::vector<DescriptorUniformData> cascadeData{
            {.buffer = cascadedShadowMapDatas[i].buffer, .allocSize = sizeof(CascadeShadowData)}
        };
        resourceManager.setupDescriptorBufferUniform(cascadedShadowMapDescriptorBufferUniform, cascadeData, i);

        // These don't actually change, but I leave them here in case there are plans to make them dynamic in the future.
        const auto data = static_cast<CascadeShadowData*>(cascadedShadowMapDatas[i].info.pMappedData);
        data->nearShadowPlane = csmProperties.cascadeNearPlane;
        data->farShadowPlane = csmProperties.cascadeFarPlane;
    }
}

CascadedShadowMap::~CascadedShadowMap()
{
    for (CascadeShadowMapData& cascadeShadowMapData : shadowMaps) {
        resourceManager.destroyResource(std::move(cascadeShadowMapData.depthShadowMap));
        cascadeShadowMapData.depthShadowMap = {};
    }

    resourceManager.destroyResource(std::move(cascadedShadowMapUniformLayout));
    resourceManager.destroyResource(std::move(cascadedShadowMapSamplerLayout));
    for (int32_t i = 0; i < FRAME_OVERLAP; ++i) {
        resourceManager.destroyResource(std::move(cascadedShadowMapDatas[i]));
    }
    resourceManager.destroyResource(std::move(cascadedShadowMapDescriptorBufferSampler));
    resourceManager.destroyResource(std::move(cascadedShadowMapDescriptorBufferUniform));
    resourceManager.destroyResource(std::move(sampler));
    resourceManager.destroyResource(std::move(renderObjectPipeline));
    resourceManager.destroyResource(std::move(renderObjectPipelineLayout));
    resourceManager.destroyResource(std::move(terrainPipeline));
    resourceManager.destroyResource(std::move(terrainPipelineLayout));
}

void CascadedShadowMap::update(const DirectionalLight& mainLight, const Camera* camera,
                               const int32_t currentFrameOverlap)
{
    for (CascadeShadowMapData& shadowData : shadowMaps) {
        shadowData.lightViewProj = getLightSpaceMatrix(shadowData.cascadeLevel, mainLight.direction, camera, shadowData.split.nearPlane,
                                                       shadowData.split.farPlane);
    }

    const Buffer& currentCascadeShadowMapData = cascadedShadowMapDatas[currentFrameOverlap];
    if (currentCascadeShadowMapData.buffer == VK_NULL_HANDLE) { return; }
    const auto data = static_cast<CascadeShadowData*>(currentCascadeShadowMapData.info.pMappedData);
    for (size_t i = 0; i < SHADOW_CASCADE_COUNT; i++) {
        data->cascadeSplits[i] = shadowMaps[i].split;
        data->lightViewProj[i] = shadowMaps[i].lightViewProj;
    }
    data->directionalLightData = mainLight.getData();

    data->nearShadowPlane = shadowMaps[0].split.nearPlane;
    data->farShadowPlane = shadowMaps[SHADOW_CASCADE_COUNT - 1].split.farPlane;
}

void CascadedShadowMap::draw(VkCommandBuffer cmd, CascadedShadowMapDrawInfo drawInfo)
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Shadow Pass";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    constexpr VkClearValue clearValue = {
        .depthStencil = {
            1.0f,
            0u
        }
    };

    for (int32_t i{0}; i < SHADOW_CASCADE_COUNT; i++) {
        CascadeShadowMapData& cascadeShadowMapData = shadowMaps[i];
        VkExtent3D cascadeExtents = csmProperties.cascadeExtents[i];
        CascadeBias cascadeBias = csmProperties.cascadeBias[i];

        if (!drawInfo.bEnabled) {
            vk_helpers::clearColorImage(cmd, VK_IMAGE_ASPECT_DEPTH_BIT, cascadeShadowMapData.depthShadowMap.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, {0.0f, 0.0f});
            continue;
        }

        vk_helpers::imageBarrier(cmd, cascadeShadowMapData.depthShadowMap.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);


        // Terrain
        {
            VkRenderingAttachmentInfo depthAttachment = vk_helpers::attachmentInfo(cascadeShadowMapData.depthShadowMap.imageView, &clearValue,
                                                                                   VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

            VkRenderingInfo renderInfo{};
            renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            renderInfo.pNext = nullptr;

            renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, {cascadeExtents.width, cascadeExtents.height}};
            renderInfo.layerCount = 1;
            renderInfo.colorAttachmentCount = 0;
            renderInfo.pColorAttachments = nullptr;
            renderInfo.pDepthAttachment = &depthAttachment;
            renderInfo.pStencilAttachment = nullptr;

            vkCmdBeginRendering(cmd, &renderInfo);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, terrainPipeline.pipeline);

            vkCmdSetDepthBias(cmd, cascadeBias.constant, 0.0f, cascadeBias.slope);

            CascadedShadowMapGenerationPushConstants pushConstants{};
            pushConstants.cascadeIndex = cascadeShadowMapData.cascadeLevel;
            pushConstants.tessLevel = 1;
            vkCmdPushConstants(cmd, terrainPipelineLayout.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 0,
                               sizeof(CascadedShadowMapGenerationPushConstants), &pushConstants);

            //  Viewport
            VkViewport viewport = {};
            viewport.x = 0;
            viewport.y = 0;
            viewport.width = cascadeExtents.width;
            viewport.height = cascadeExtents.height;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
            vkCmdSetViewport(cmd, 0, 1, &viewport);
            //  Scissor
            VkRect2D scissor = {};
            scissor.offset.x = 0;
            scissor.offset.y = 0;
            scissor.extent.width = cascadeExtents.width;
            scissor.extent.height = cascadeExtents.height;
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            constexpr VkDeviceSize zeroOffset{0};

            for (ITerrain* terrain : drawInfo.terrains) {
                terrain::TerrainChunk* terrainChunk = terrain->getTerrainChunk();
                if (!terrainChunk) { continue; }

                VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[1];
                constexpr uint32_t shadowDataIndex{0};
                descriptorBufferBindingInfo[0] = cascadedShadowMapDescriptorBufferUniform.getBindingInfo();

                vkCmdBindDescriptorBuffersEXT(cmd, 1, descriptorBufferBindingInfo);

                const VkDeviceSize shadowDataOffset{
                    cascadedShadowMapDescriptorBufferUniform.getDescriptorBufferSize() * drawInfo.currentFrameOverlap
                };
                vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, terrainPipelineLayout.layout, 0, 1, &shadowDataIndex,
                                                   &shadowDataOffset);

                VkBuffer vertexBuffer = terrainChunk->getVertexBuffer().buffer;
                vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &zeroOffset);
                vkCmdBindIndexBuffer(cmd, terrainChunk->getIndexBuffer().buffer, 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(cmd, terrainChunk->getIndexCount(), 1, 0, 0, 0);
            }

            vkCmdEndRendering(cmd);
        }

        // Render Objects
        {
            VkRenderingAttachmentInfo depthAttachment = vk_helpers::attachmentInfo(cascadeShadowMapData.depthShadowMap.imageView, nullptr,
                                                                                   VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

            VkRenderingInfo renderInfo{};
            renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            renderInfo.pNext = nullptr;

            renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, {cascadeExtents.width, cascadeExtents.height}};
            renderInfo.layerCount = 1;
            renderInfo.colorAttachmentCount = 0;
            renderInfo.pColorAttachments = nullptr;
            renderInfo.pDepthAttachment = &depthAttachment;
            renderInfo.pStencilAttachment = nullptr;

            vkCmdBeginRendering(cmd, &renderInfo);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderObjectPipeline.pipeline);

            vkCmdSetDepthBias(cmd, cascadeBias.constant, 0.0f, cascadeBias.slope);

            CascadedShadowMapGenerationPushConstants pushConstants{};
            pushConstants.cascadeIndex = cascadeShadowMapData.cascadeLevel;
            vkCmdPushConstants(cmd, renderObjectPipelineLayout.layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                               sizeof(CascadedShadowMapGenerationPushConstants),
                               &pushConstants);

            //  Viewport
            VkViewport viewport = {};
            viewport.x = 0;
            viewport.y = 0;
            viewport.width = cascadeExtents.width;
            viewport.height = cascadeExtents.height;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
            vkCmdSetViewport(cmd, 0, 1, &viewport);
            //  Scissor
            VkRect2D scissor = {};
            scissor.offset.x = 0;
            scissor.offset.y = 0;
            scissor.extent.width = cascadeExtents.width;
            scissor.extent.height = cascadeExtents.height;
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            constexpr VkDeviceSize zeroOffset{0};

            for (RenderObject* renderObject : drawInfo.renderObjects) {
                if (!renderObject->canDrawOpaque()) { continue; }

                std::array bindings{
                    cascadedShadowMapDescriptorBufferUniform.getBindingInfo(),
                    renderObject->getAddressesDescriptorBuffer().getBindingInfo()
                };
                vkCmdBindDescriptorBuffersEXT(cmd, 2, bindings.data());

                std::array indices{0u, 1u};
                std::array offests{
                    cascadedShadowMapDescriptorBufferUniform.getDescriptorBufferSize() * drawInfo.currentFrameOverlap,
                    renderObject->getAddressesDescriptorBuffer().getDescriptorBufferSize() * drawInfo.currentFrameOverlap
                };

                vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderObjectPipelineLayout.layout, 0, 2, indices.data(),
                                                   offests.data());


                vkCmdBindVertexBuffers(cmd, 0, 1, &renderObject->getPositionVertexBuffer().buffer, &zeroOffset);
                vkCmdBindIndexBuffer(cmd, renderObject->getIndexBuffer().buffer, 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexedIndirect(cmd, renderObject->getOpaqueIndirectBuffer(drawInfo.currentFrameOverlap).buffer, 0,
                                         renderObject->getOpaqueDrawIndirectCommandCount(), sizeof(VkDrawIndexedIndirectCommand));
            }

            vkCmdEndRendering(cmd);
        }

        vk_helpers::imageBarrier(cmd, cascadeShadowMapData.depthShadowMap.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
    }


    vkCmdEndDebugUtilsLabelEXT(cmd);
}


glm::mat4 CascadedShadowMap::getLightSpaceMatrix(int32_t cascadeLevel, const glm::vec3 lightDirection,
                                                 const Camera* camera, float cascadeNear, float cascadeFar,
                                                 bool reversedDepth) const
{
    constexpr int32_t numberOfCorners = 8;
    glm::vec3 corners[numberOfCorners];
    render_utils::getPerspectiveFrustumCornersWorldSpace(cascadeNear, cascadeFar, camera->getFov(), camera->getAspectRatio(), camera->getPosition(),
                                                         camera->getForwardWS(), corners);

    // Alternative more expensive way to calculate corners
    // auto viewMatrix = camera->getViewMatrix();
    // auto projMatrix = glm::perspective(camera->getFov(), camera->getAspectRatio(), cascadeNear, cascadeFar);
    // auto viewProj = projMatrix * viewMatrix;
    // glm::vec3 corners2[numberOfCorners];
    // render_utils::getPerspectiveFrustumCornersWorldSpace(viewProj, corners2);

    // https://alextardif.com/shadowmapping.html
    auto frustumCenter = glm::vec3(0.0f);
    for (const glm::vec3& corner : corners) {
        frustumCenter += corner;
    }
    frustumCenter = frustumCenter * (1.0f / numberOfCorners);

    float maxDistanceSquared = 0.0f;
    for (const glm::vec3& corner : corners) {
        float distanceSquared = glm::length2(corner - frustumCenter);
        maxDistanceSquared = std::max(maxDistanceSquared, distanceSquared);
    }

    float radius = std::sqrt(maxDistanceSquared);

    VkExtent3D cascadeExtents = csmProperties.cascadeExtents[cascadeLevel];
    float texelsPerUnit = cascadeExtents.width / glm::max(radius * 2.0f, 1.0f);

    const glm::mat4 scaleMatrix = scale(glm::mat4(1.0f), glm::vec3(texelsPerUnit));
    glm::mat4 view = glm::lookAt(-lightDirection, glm::vec3(0.0f), GLOBAL_UP);
    view = scaleMatrix * view;
    glm::mat4 invView = glm::inverse(view);

    glm::vec4 tempFrustumCenter = glm::vec4(frustumCenter, 1.0f);
    tempFrustumCenter = view * tempFrustumCenter;
    tempFrustumCenter.x = glm::floor(tempFrustumCenter.x);
    tempFrustumCenter.y = glm::floor(tempFrustumCenter.y);
    frustumCenter = glm::vec3(invView * tempFrustumCenter);

    glm::vec3 eye = frustumCenter - (lightDirection * radius * 2.0f);

    glm::mat4 lightView = lookAt(eye, frustumCenter, GLOBAL_UP);
    constexpr float zMult = 10.0f;
    glm::mat4 lightProj = glm::ortho(-radius, radius, -radius, radius, -radius * zMult, radius * zMult);
    if (reversedDepth) {
        lightProj = glm::ortho(-radius, radius, -radius, radius, radius * zMult, -radius * zMult);
    }

    return lightProj * lightView;
}


void CascadedShadowMap::createRenderObjectPipeline()
{
    resourceManager.destroyResource(std::move(renderObjectPipeline));

    VkShaderModule vertShader = resourceManager.createShaderModule("shaders/shadows/shadow_pass.vert");
    VkShaderModule fragShader = resourceManager.createShaderModule("shaders/shadows/shadow_pass.frag");

    RenderPipelineBuilder pipelineBuilder;
    const std::vector<VkVertexInputBindingDescription> vertexBindings{
        {
            .binding = 0,
            .stride = sizeof(VertexPosition),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        }
    };

    const std::vector<VkVertexInputAttributeDescription> vertexAttributes{
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(VertexPosition, position),
        }
    };

    pipelineBuilder.setupVertexInput(vertexBindings, vertexAttributes);

    pipelineBuilder.setShaders(vertShader, fragShader);
    pipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    // set later during shadow pass
    pipelineBuilder.enableDepthBias(0.0f, 0, 0.0f);
    pipelineBuilder.disableMultisampling();
    pipelineBuilder.disableBlending();
    pipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
    pipelineBuilder.setupRenderer({}, CASCADE_DEPTH_FORMAT);
    pipelineBuilder.setupPipelineLayout(renderObjectPipelineLayout.layout);
    pipelineBuilder.addDynamicState(VK_DYNAMIC_STATE_DEPTH_BIAS);

    renderObjectPipeline = resourceManager.createRenderPipeline(pipelineBuilder);
    resourceManager.destroyShaderModule(vertShader);
    resourceManager.destroyShaderModule(fragShader);
}

void CascadedShadowMap::createTerrainPipeline()
{
    resourceManager.destroyResource(std::move(terrainPipeline));

    VkShaderModule vertShader = resourceManager.createShaderModule("shaders/shadows/terrain_shadow_pass.vert");
    VkShaderModule tescShader = resourceManager.createShaderModule("shaders/shadows/terrain_shadow_pass.tesc");
    VkShaderModule teseShader = resourceManager.createShaderModule("shaders/shadows/terrain_shadow_pass.tese");
    VkShaderModule fragShader = resourceManager.createShaderModule("shaders/shadows/shadow_pass.frag");


    RenderPipelineBuilder pipelineBuilder;
    const std::vector<VkVertexInputBindingDescription> vertexBindings{
        {
            .binding = 0,
            .stride = sizeof(terrain::TerrainVertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        }
    };

    const std::vector<VkVertexInputAttributeDescription> vertexAttributes{
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(terrain::TerrainVertex, position),
        }
    };

    pipelineBuilder.setupVertexInput(vertexBindings, vertexAttributes);

    pipelineBuilder.setShaders(vertShader, tescShader, teseShader, fragShader);
    pipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, false);
    pipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    // set later during shadow pass
    pipelineBuilder.enableDepthBias(0.0f, 0, 0.0f);
    pipelineBuilder.disableMultisampling();
    pipelineBuilder.disableBlending();
    pipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
    pipelineBuilder.setupRenderer({}, CASCADE_DEPTH_FORMAT);
    pipelineBuilder.setupPipelineLayout(terrainPipelineLayout.layout);
    pipelineBuilder.setupTessellation(4);
    pipelineBuilder.addDynamicState(VK_DYNAMIC_STATE_DEPTH_BIAS);

    terrainPipeline = resourceManager.createRenderPipeline(pipelineBuilder);
    resourceManager.destroyShaderModule(vertShader);
    resourceManager.destroyShaderModule(tescShader);
    resourceManager.destroyShaderModule(teseShader);
    resourceManager.destroyShaderModule(fragShader);
}

void CascadedShadowMap::generateSplits()
{
    if (csmProperties.useManualSplit) {
        for (int32_t i = 0; i < SHADOW_CASCADE_COUNT; ++i) {
            shadowMaps[i].split.nearPlane = csmProperties.manualCascadeSplits[i];
            shadowMaps[i].split.farPlane = csmProperties.manualCascadeSplits[i + 1];
        }
    }
    else {
        // https://github.com/diharaw/cascaded-shadow-maps
        // Practical Split Scheme: https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html

        const float ratio = csmProperties.cascadeFarPlane / csmProperties.cascadeNearPlane;
        shadowMaps[0].split.nearPlane = csmProperties.cascadeNearPlane;

        for (size_t i = 1; i < SHADOW_CASCADE_COUNT; i++) {
            const float si = static_cast<float>(i) / static_cast<float>(SHADOW_CASCADE_COUNT);

            const float uniformTerm = csmProperties.cascadeNearPlane + (csmProperties.cascadeFarPlane - csmProperties.cascadeNearPlane) * si;
            const float logTerm = csmProperties.cascadeNearPlane * std::pow(ratio, si);
            const float nearValue = csmProperties.splitLambda * logTerm + (1.0f - csmProperties.splitLambda) * uniformTerm;

            const float farValue = nearValue * csmProperties.splitOverlap;

            shadowMaps[i].split.nearPlane = nearValue;
            shadowMaps[i - 1].split.farPlane = farValue;
        }

        shadowMaps[SHADOW_CASCADE_COUNT - 1].split.farPlane = csmProperties.cascadeFarPlane;
    }
}
}
