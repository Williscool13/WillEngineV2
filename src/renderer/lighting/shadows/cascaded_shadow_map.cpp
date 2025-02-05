//
// Created by William on 2025-01-26.
//

#include "cascaded_shadow_map.h"

#include "volk.h"
#include "src/core/camera/camera.h"
#include "src/renderer/renderer_constants.h"
#include "src/renderer/render_object/render_object_types.h"
#include "src/util/render_utils.h"


will_engine::cascaded_shadows::CascadedShadowMap::CascadedShadowMap(ResourceManager& resourceManager)
    : resourceManager(resourceManager)
{
    // (Uniform) Shadow Map Layout - Used by deferred resolve
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        cascadedShadowMapUniformLayout = resourceManager.createDescriptorSetLayout(layoutBuilder, static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT),
                                                                                   VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    }

    // (Sampler) Shadow Map Layout - Used by deferred resolve
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, shadows::SHADOW_CASCADE_COUNT);
        cascadedShadowMapSamplerLayout = resourceManager.createDescriptorSetLayout(layoutBuilder, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
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

    for (CascadeShadowMap& cascadeShadowMapData : shadowMaps) {
        cascadeShadowMapData.depthShadowMap = resourceManager.createImage({shadows::CASCADE_EXTENT.width, shadows::CASCADE_EXTENT.height, 1}, shadows::CASCADE_DEPTH_FORMAT,
                                                                          VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    }


    VkDescriptorSetLayout layouts[2];
    layouts[0] = cascadedShadowMapUniformLayout;
    layouts[1] = resourceManager.getAddressesLayout();

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

    pipelineLayout = resourceManager.createPipelineLayout(layoutInfo);


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
    pipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    // set later during shadow pass
    pipelineBuilder.enableDepthBias(0.0f, 0, 0.0f);
    pipelineBuilder.disableMultisampling();
    pipelineBuilder.setupBlending(PipelineBuilder::BlendMode::NO_BLEND);
    pipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
    pipelineBuilder.setupRenderer({}, DEPTH_FORMAT);
    pipelineBuilder.setupPipelineLayout(pipelineLayout);

    pipeline = resourceManager.createRenderPipeline(pipelineBuilder, {VK_DYNAMIC_STATE_DEPTH_BIAS});
    resourceManager.destroyShaderModule(vertShader);
    resourceManager.destroyShaderModule(fragShader);


    std::vector<DescriptorImageData> textureDescriptors;
    for (const CascadeShadowMap& shadowMapData : shadowMaps) {
        const VkDescriptorImageInfo imageInfo{.sampler = sampler, .imageView = shadowMapData.depthShadowMap.imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
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
    for (CascadeShadowMap& cascadeShadowMapData : shadowMaps) {
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
    resourceManager.destroyPipeline(pipeline);
    resourceManager.destroyPipelineLayout(pipelineLayout);
}

void will_engine::cascaded_shadows::CascadedShadowMap::update(const DirectionalLight& mainLight, const Camera* camera, const int32_t currentFrameOverlap)
{
    for (CascadeShadowMap& shadowData : shadowMaps) {
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

void will_engine::cascaded_shadows::CascadedShadowMap::draw(VkCommandBuffer cmd, const std::vector<RenderObject*>& renderObjects, const int32_t currentFrameOverlap)
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "Shadow Pass";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    constexpr VkClearValue clearValue = {1.0f, 1.0f};

    for (const CascadeShadowMap& cascadeShadowMapData : shadowMaps) {
        vk_helpers::transitionImage(cmd, cascadeShadowMapData.depthShadowMap.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

        VkRenderingAttachmentInfo depthAttachment = vk_helpers::attachmentInfo(cascadeShadowMapData.depthShadowMap.imageView, &clearValue, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

        VkRenderingInfo renderInfo{};
        renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderInfo.pNext = nullptr;

        renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, shadows::CASCADE_EXTENT};
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = 0;
        renderInfo.pColorAttachments = nullptr;
        renderInfo.pDepthAttachment = &depthAttachment;
        renderInfo.pStencilAttachment = nullptr;

        vkCmdBeginRendering(cmd, &renderInfo);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        vkCmdSetDepthBias(cmd, shadows::CASCADE_BIAS[cascadeShadowMapData.cascadeLevel][0], 0.0f, shadows::CASCADE_BIAS[cascadeShadowMapData.cascadeLevel][1]);

        CascadedShadowMapGenerationPushConstants pushConstants{};
        pushConstants.cascadeIndex = cascadeShadowMapData.cascadeLevel;
        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(CascadedShadowMapGenerationPushConstants), &pushConstants);

        //  Viewport
        VkViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = shadows::CASCADE_EXTENT.width;
        viewport.height = shadows::CASCADE_EXTENT.height;
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        //  Scissor
        VkRect2D scissor = {};
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = shadows::CASCADE_EXTENT.width;
        scissor.extent.height = shadows::CASCADE_EXTENT.height;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        constexpr VkDeviceSize zeroOffset{0};

        for (const RenderObject* renderObject : renderObjects) {
            if (!renderObject->canDraw()) { continue; }

            VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[2];
            constexpr uint32_t shadowDataIndex{0};
            constexpr uint32_t addressIndex{1};
            descriptorBufferBindingInfo[0] = cascadedShadowMapDescriptorBufferUniform.getDescriptorBufferBindingInfo();
            descriptorBufferBindingInfo[1] = renderObject->getAddressesDescriptorBuffer().getDescriptorBufferBindingInfo();

            vkCmdBindDescriptorBuffersEXT(cmd, 2, descriptorBufferBindingInfo);

            const VkDeviceSize shadowDataOffset{cascadedShadowMapDescriptorBufferUniform.getDescriptorBufferSize() * currentFrameOverlap};
            const VkDeviceSize addressOffset{renderObject->getAddressesDescriptorBuffer().getDescriptorBufferSize() * currentFrameOverlap};
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


glm::mat4 will_engine::cascaded_shadows::CascadedShadowMap::getLightSpaceMatrix(const glm::vec3 lightDirection, const Camera* camera, float cascadeNear, float cascadeFar)
{
    constexpr int32_t numberOfCorners = 8;
    glm::vec4 corners[numberOfCorners];
    render_utils::getPerspectiveFrustumCornersWorldSpace(cascadeNear, cascadeFar, camera->getFov(), camera->getAspectRatio(), camera->getPosition(), camera->getForwardWS(), corners);

    glm::vec3 center{0.0f};
    for (const auto& v : corners) {
        center += glm::vec3(v);
    }
    center /= numberOfCorners;

    const auto lightView = glm::lookAt(
        center - lightDirection,
        center,
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
    constexpr float zMult = 10.0f;
    minZ *= zMult;
    maxZ *= zMult;
    float cascadeBound = cascadeFar - cascadeNear;
    float worldUnitsPerTexel = cascadeBound / static_cast<float>(will_engine::shadows::CASCADE_EXTENT.width);
    glm::vec3 minBounds{minX, minY, minZ};
    glm::vec3 maxBounds{maxX, maxY, maxZ};
    minBounds = glm::floor(minBounds / worldUnitsPerTexel) * worldUnitsPerTexel;
    maxBounds = glm::floor(maxBounds / worldUnitsPerTexel) * worldUnitsPerTexel;
    const glm::mat4 lightProjection = glm::ortho(minBounds.x, maxBounds.x, minBounds.y, maxBounds.y, minBounds.z, maxBounds.z);
    return lightProjection * lightView;


    //const glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
    //return lightProjection * lightView;
}
