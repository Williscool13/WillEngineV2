//
// Created by William on 2025-01-26.
//

#include "cascaded_shadow_map.h"

#include <ranges>

#include "volk/volk.h"
#include "src/core/camera/camera.h"
#include "src/renderer/renderer_constants.h"
#include "src/renderer/pipelines/terrain/terrain_pipeline.h"
#include "src/renderer/assets/render_object/render_object_types.h"
#include "src/renderer/terrain/terrain_chunk.h"
#include "src/util/math_constants.h"
#include "src/util/render_utils.h"


void will_engine::cascaded_shadows::CascadedShadowMap::createRenderObjectPipeline()
{
    resourceManager.destroyPipeline(renderObjectPipeline);

    VkShaderModule vertShader = resourceManager.createShaderModule("shaders/shadows/shadow_pass.vert");
    VkShaderModule fragShader = resourceManager.createShaderModule("shaders/shadows/shadow_pass.frag");

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
    // set later during shadow pass
    pipelineBuilder.enableDepthBias(0.0f, 0, 0.0f);
    pipelineBuilder.disableMultisampling();
    pipelineBuilder.disableBlending();
    pipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
    pipelineBuilder.setupRenderer({}, DEPTH_FORMAT);
    pipelineBuilder.setupPipelineLayout(renderObjectPipelineLayout);

    renderObjectPipeline = resourceManager.createRenderPipeline(pipelineBuilder, {VK_DYNAMIC_STATE_DEPTH_BIAS});
    resourceManager.destroyShaderModule(vertShader);
    resourceManager.destroyShaderModule(fragShader);
}

void will_engine::cascaded_shadows::CascadedShadowMap::createTerrainPipeline()
{
    resourceManager.destroyPipeline(terrainPipeline);

    VkShaderModule vertShader = resourceManager.createShaderModule("shaders/shadows/terrain_shadow_pass.vert");
    VkShaderModule tescShader = resourceManager.createShaderModule("shaders/shadows/terrain_shadow_pass.tesc");
    VkShaderModule teseShader = resourceManager.createShaderModule("shaders/shadows/terrain_shadow_pass.tese");
    VkShaderModule fragShader = resourceManager.createShaderModule("shaders/shadows/shadow_pass.frag");

    PipelineBuilder pipelineBuilder;
    VkVertexInputBindingDescription mainBinding{};
    mainBinding.binding = 0;
    mainBinding.stride = sizeof(terrain::TerrainVertex);
    mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription vertexAttributes[1];
    vertexAttributes[0].binding = 0;
    vertexAttributes[0].location = 0;
    vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexAttributes[0].offset = offsetof(terrain::TerrainVertex, position);

    pipelineBuilder.setupVertexInput(&mainBinding, 1, vertexAttributes, 1);

    pipelineBuilder.setShaders(vertShader, tescShader, teseShader, fragShader);
    pipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, false);
    pipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    // set later during shadow pass
    pipelineBuilder.enableDepthBias(0.0f, 0, 0.0f);
    pipelineBuilder.disableMultisampling();
    pipelineBuilder.disableBlending();
    pipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
    pipelineBuilder.setupRenderer({}, DEPTH_FORMAT);
    pipelineBuilder.setupPipelineLayout(terrainPipelineLayout);
    pipelineBuilder.setupTessellation(4);

    terrainPipeline = resourceManager.createRenderPipeline(pipelineBuilder, {VK_DYNAMIC_STATE_DEPTH_BIAS});
    resourceManager.destroyShaderModule(vertShader);
    resourceManager.destroyShaderModule(tescShader);
    resourceManager.destroyShaderModule(teseShader);
    resourceManager.destroyShaderModule(fragShader);
}

will_engine::cascaded_shadows::CascadedShadowMap::CascadedShadowMap(ResourceManager& resourceManager)
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
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, shadows::SHADOW_CASCADE_COUNT);
        cascadedShadowMapSamplerLayout = resourceManager.createDescriptorSetLayout(
            layoutBuilder,
            static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }

    // https://github.com/diharaw/cascaded-shadow-maps
    // Practical Split Scheme: https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html

    constexpr float ratio = shadows::CASCADE_FAR_PLANE / shadows::CASCADE_NEAR_PLANE;
    shadowMaps[0].split.nearPlane = shadows::CASCADE_NEAR_PLANE;

    fmt::print("Shadow Map: (Splits) {} - ", shadowMaps[0].split.nearPlane);

    for (size_t i = 1; i < shadows::SHADOW_CASCADE_COUNT; i++) {
        const float si = static_cast<float>(i) / static_cast<float>(shadows::SHADOW_CASCADE_COUNT);

        const float uniformTerm = shadows::CASCADE_NEAR_PLANE + (shadows::CASCADE_FAR_PLANE - shadows::CASCADE_NEAR_PLANE) * si;
        const float logTerm = shadows::CASCADE_NEAR_PLANE * std::pow(ratio, si);
        const float nearValue = shadows::LAMBDA * logTerm + (1.0f - shadows::LAMBDA) * uniformTerm;

        const float farValue = nearValue * shadows::OVERLAP;

        shadowMaps[i].split.nearPlane = nearValue;
        shadowMaps[i - 1].split.farPlane = farValue;

        fmt::print("{} - ", shadowMaps[i].split.nearPlane);
    }

    shadowMaps[shadows::SHADOW_CASCADE_COUNT - 1].split.farPlane = shadows::CASCADE_FAR_PLANE;
    fmt::print("{}\n", shadowMaps[shadows::SHADOW_CASCADE_COUNT - 1].split.farPlane);


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

    for (CascadeShadowMapData& cascadeShadowMapData : shadowMaps) {
        VkImageUsageFlags usage{};
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        VkImageCreateInfo imgInfo = vk_helpers::imageCreateInfo(shadows::CASCADE_DEPTH_FORMAT, usage, {
                                                                    shadows::CASCADE_WIDTH, shadows::CASCADE_HEIGHT, 1
                                                                });
        cascadeShadowMapData.depthShadowMap = resourceManager.createImage(imgInfo);
    }

    //
    {
        VkDescriptorSetLayout layouts[2];
        layouts[0] = cascadedShadowMapUniformLayout;
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
        layouts[0] = cascadedShadowMapUniformLayout;

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
            .sampler = sampler, .imageView = shadowMapData.depthShadowMap.imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        textureDescriptors.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfo, false});
    }
    cascadedShadowMapDescriptorBufferSampler = resourceManager.createDescriptorBufferSampler(cascadedShadowMapSamplerLayout, 1);
    resourceManager.setupDescriptorBufferSampler(cascadedShadowMapDescriptorBufferSampler, textureDescriptors, 0);


    cascadedShadowMapDescriptorBufferUniform = resourceManager.createDescriptorBufferUniform(cascadedShadowMapUniformLayout, FRAME_OVERLAP);
    for (int32_t i = 0; i < FRAME_OVERLAP; ++i) {
        cascadedShadowMapDatas[i] = resourceManager.createHostSequentialBuffer(sizeof(CascadeShadowData));
        std::vector<DescriptorUniformData> cascadeData{
            {.uniformBuffer = cascadedShadowMapDatas[i], .allocSize = sizeof(CascadeShadowData)}
        };
        resourceManager.setupDescriptorBufferUniform(cascadedShadowMapDescriptorBufferUniform, cascadeData, i);

        // These don't actually change, but I leave them here in case there are plans to make them dynamic in the future.
        const auto data = static_cast<CascadeShadowData*>(cascadedShadowMapDatas[i].info.pMappedData);
        data->nearShadowPlane = shadows::CASCADE_NEAR_PLANE;
        data->farShadowPlane = shadows::CASCADE_FAR_PLANE;
    }
}

will_engine::cascaded_shadows::CascadedShadowMap::~CascadedShadowMap()
{
    for (CascadeShadowMapData& cascadeShadowMapData : shadowMaps) {
        resourceManager.destroyImage(cascadeShadowMapData.depthShadowMap);
        cascadeShadowMapData.depthShadowMap = {};
    }

    resourceManager.destroyDescriptorSetLayout(cascadedShadowMapUniformLayout);
    resourceManager.destroyDescriptorSetLayout(cascadedShadowMapSamplerLayout);
    for (int32_t i = 0; i < FRAME_OVERLAP; ++i) {
        resourceManager.destroyBuffer(cascadedShadowMapDatas[i]);
    }
    resourceManager.destroyDescriptorBuffer(cascadedShadowMapDescriptorBufferSampler);
    resourceManager.destroyDescriptorBuffer(cascadedShadowMapDescriptorBufferUniform);
    resourceManager.destroySampler(sampler);
    resourceManager.destroyPipeline(renderObjectPipeline);
    resourceManager.destroyPipelineLayout(renderObjectPipelineLayout);
    resourceManager.destroyPipeline(terrainPipeline);
    resourceManager.destroyPipelineLayout(terrainPipelineLayout);
}

void will_engine::cascaded_shadows::CascadedShadowMap::update(const DirectionalLight& mainLight, const Camera* camera,
                                                              const int32_t currentFrameOverlap)
{
    for (CascadeShadowMapData& shadowData : shadowMaps) {
        shadowData.lightViewProj = getLightSpaceMatrix(mainLight.getDirection(), camera, shadowData.split.nearPlane, shadowData.split.farPlane);
    }

    const AllocatedBuffer& currentCascadeShadowMapData = cascadedShadowMapDatas[currentFrameOverlap];
    if (currentCascadeShadowMapData.buffer == VK_NULL_HANDLE) { return; }
    const auto data = static_cast<CascadeShadowData*>(currentCascadeShadowMapData.info.pMappedData);
    for (size_t i = 0; i < shadows::SHADOW_CASCADE_COUNT; i++) {
        data->cascadeSplits[i] = shadowMaps[i].split;
        data->lightViewProj[i] = shadowMaps[i].lightViewProj;
    }
    data->directionalLightData = mainLight.getData();
}

void will_engine::cascaded_shadows::CascadedShadowMap::draw(VkCommandBuffer cmd, const std::vector<RenderObject*>& renderObjects,
                                                            const std::unordered_set<ITerrain*>& terrains, const int32_t currentFrameOverlap)
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

    for (const CascadeShadowMapData& cascadeShadowMapData : shadowMaps) {
        vk_helpers::transitionImage(cmd, cascadeShadowMapData.depthShadowMap.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                    VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);


        // Terrain
        {
            VkRenderingAttachmentInfo depthAttachment = vk_helpers::attachmentInfo(cascadeShadowMapData.depthShadowMap.imageView, &clearValue,
                                                                                   VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

            VkRenderingInfo renderInfo{};
            renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            renderInfo.pNext = nullptr;

            renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, {shadows::CASCADE_WIDTH, shadows::CASCADE_HEIGHT}};
            renderInfo.layerCount = 1;
            renderInfo.colorAttachmentCount = 0;
            renderInfo.pColorAttachments = nullptr;
            renderInfo.pDepthAttachment = &depthAttachment;
            renderInfo.pStencilAttachment = nullptr;

            vkCmdBeginRendering(cmd, &renderInfo);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, terrainPipeline);

            vkCmdSetDepthBias(cmd, shadows::CASCADE_BIAS[cascadeShadowMapData.cascadeLevel][0], 0.0f,
                              shadows::CASCADE_BIAS[cascadeShadowMapData.cascadeLevel][1]);

            CascadedShadowMapGenerationPushConstants pushConstants{};
            pushConstants.cascadeIndex = cascadeShadowMapData.cascadeLevel;
            pushConstants.tessLevel = 1;
            vkCmdPushConstants(cmd, terrainPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 0,
                               sizeof(CascadedShadowMapGenerationPushConstants), &pushConstants);

            //  Viewport
            VkViewport viewport = {};
            viewport.x = 0;
            viewport.y = 0;
            viewport.width = shadows::CASCADE_WIDTH;
            viewport.height = shadows::CASCADE_HEIGHT;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
            vkCmdSetViewport(cmd, 0, 1, &viewport);
            //  Scissor
            VkRect2D scissor = {};
            scissor.offset.x = 0;
            scissor.offset.y = 0;
            scissor.extent.width = shadows::CASCADE_WIDTH;
            scissor.extent.height = shadows::CASCADE_HEIGHT;
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            constexpr VkDeviceSize zeroOffset{0};

            for (ITerrain* terrain : terrains) {
                terrain::TerrainChunk* terrainChunk = terrain->getTerrainChunk();
                if (!terrainChunk) { continue; }

                VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[1];
                constexpr uint32_t shadowDataIndex{0};
                descriptorBufferBindingInfo[0] = cascadedShadowMapDescriptorBufferUniform.getDescriptorBufferBindingInfo();

                vkCmdBindDescriptorBuffersEXT(cmd, 1, descriptorBufferBindingInfo);

                const VkDeviceSize shadowDataOffset{cascadedShadowMapDescriptorBufferUniform.getDescriptorBufferSize() * currentFrameOverlap};
                vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, terrainPipelineLayout, 0, 1, &shadowDataIndex,
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

            renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, {shadows::CASCADE_WIDTH, shadows::CASCADE_HEIGHT}};
            renderInfo.layerCount = 1;
            renderInfo.colorAttachmentCount = 0;
            renderInfo.pColorAttachments = nullptr;
            renderInfo.pDepthAttachment = &depthAttachment;
            renderInfo.pStencilAttachment = nullptr;

            vkCmdBeginRendering(cmd, &renderInfo);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderObjectPipeline);

            vkCmdSetDepthBias(cmd, shadows::CASCADE_BIAS[cascadeShadowMapData.cascadeLevel][0], 0.0f,
                              shadows::CASCADE_BIAS[cascadeShadowMapData.cascadeLevel][1]);

            CascadedShadowMapGenerationPushConstants pushConstants{};
            pushConstants.cascadeIndex = cascadeShadowMapData.cascadeLevel;
            vkCmdPushConstants(cmd, renderObjectPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(CascadedShadowMapGenerationPushConstants),
                               &pushConstants);

            //  Viewport
            VkViewport viewport = {};
            viewport.x = 0;
            viewport.y = 0;
            viewport.width = shadows::CASCADE_WIDTH;
            viewport.height = shadows::CASCADE_HEIGHT;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
            vkCmdSetViewport(cmd, 0, 1, &viewport);
            //  Scissor
            VkRect2D scissor = {};
            scissor.offset.x = 0;
            scissor.offset.y = 0;
            scissor.extent.width = shadows::CASCADE_WIDTH;
            scissor.extent.height = shadows::CASCADE_HEIGHT;
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            constexpr VkDeviceSize zeroOffset{0};

            for (RenderObject* renderObject : renderObjects) {
                if (!renderObject->canDrawOpaque()) { continue; }

                VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[2];
                constexpr uint32_t shadowDataIndex{0};
                constexpr uint32_t addressIndex{1};
                descriptorBufferBindingInfo[0] = cascadedShadowMapDescriptorBufferUniform.getDescriptorBufferBindingInfo();
                descriptorBufferBindingInfo[1] = renderObject->getAddressesDescriptorBuffer().getDescriptorBufferBindingInfo();

                vkCmdBindDescriptorBuffersEXT(cmd, 2, descriptorBufferBindingInfo);

                const VkDeviceSize shadowDataOffset{cascadedShadowMapDescriptorBufferUniform.getDescriptorBufferSize() * currentFrameOverlap};
                const VkDeviceSize addressOffset{renderObject->getAddressesDescriptorBuffer().getDescriptorBufferSize() * currentFrameOverlap};
                vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderObjectPipelineLayout, 0, 1, &shadowDataIndex,
                                                   &shadowDataOffset);
                vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderObjectPipelineLayout, 1, 1, &addressIndex,
                                                   &addressOffset);


                vkCmdBindVertexBuffers(cmd, 0, 1, &renderObject->getVertexBuffer().buffer, &zeroOffset);
                vkCmdBindIndexBuffer(cmd, renderObject->getIndexBuffer().buffer, 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexedIndirect(cmd, renderObject->getOpaqueIndirectBuffer(currentFrameOverlap).buffer, 0,
                                         renderObject->getOpaqueDrawIndirectCommandCount(), sizeof(VkDrawIndexedIndirectCommand));
            }

            vkCmdEndRendering(cmd);
        }

        vk_helpers::transitionImage(cmd, cascadeShadowMapData.depthShadowMap.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
    }


    vkCmdEndDebugUtilsLabelEXT(cmd);
}


glm::mat4 will_engine::cascaded_shadows::CascadedShadowMap::getLightSpaceMatrix(const glm::vec3 lightDirection, const Camera* camera,
                                                                                float cascadeNear, float cascadeFar, bool reversedDepth)
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

    assert(shadows::CASCADE_HEIGHT == shadows::CASCADE_WIDTH);
    float texelsPerUnit = shadows::CASCADE_WIDTH / glm::max(radius * 2.0f, 1.0f);

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
    constexpr float zMult = 6.0f;
    glm::mat4 lightProj = glm::ortho(-radius, radius, -radius, radius, -radius * zMult, radius * zMult);
    if (reversedDepth) {
        lightProj = glm::ortho(-radius, radius, -radius, radius, radius * zMult, -radius * zMult);
    }

    return lightProj * lightView;
}
