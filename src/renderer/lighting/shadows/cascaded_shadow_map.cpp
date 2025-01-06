//
// Created by William on 2025-01-01.
//

#include "cascaded_shadow_map.h"

#include "shadow_map_descriptor_layouts.h"
#include "shadow_types.h"
#include "src/core/camera/camera.h"
#include "src/renderer/resource_manager.h"
#include "src/renderer/vk_pipelines.h"

CascadedShadowMap::CascadedShadowMap(const VulkanContext& context, ResourceManager& resourceManager)
    : context(context), resourceManager(resourceManager)
{}

CascadedShadowMap::~CascadedShadowMap()
{
    for (AllocatedImage& shadowMap : shadowMaps) {
        resourceManager.destroyImage(shadowMap);
        shadowMap = {};
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
        VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

        sampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampl.minLod = 0;
        sampl.maxLod = VK_LOD_CLAMP_NONE;
        sampl.magFilter = VK_FILTER_LINEAR;
        sampl.minFilter = VK_FILTER_LINEAR;

        sampl.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampl.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampl.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        sampl.compareEnable = VK_TRUE;
        sampl.compareOp = VK_COMPARE_OP_LESS;

        sampl.anisotropyEnable = VK_TRUE;
        sampl.maxAnisotropy = 16.0f;

        vkCreateSampler(context.device, &sampl, nullptr, &sampler);

        //glm::ortho()
        for (AllocatedImage& shadowMap : shadowMaps) {
            shadowMap = resourceManager.createImage({extent.width, extent.height, 1}, depthFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
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
        for (AllocatedImage& shadowMap : shadowMaps) {
            VkDescriptorImageInfo imageInfo{.imageView = shadowMap.imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            textureDescriptors.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfo, false});
        }
        shadowMapDescriptorBuffer = DescriptorBufferSampler(context, shadowMapPipelineCreateInfo.shadowMapLayout, 1);
        shadowMapDescriptorBuffer.setupData(context.device, textureDescriptors);
    }
}

void CascadedShadowMap::draw(VkCommandBuffer cmd)
{
    // todo: Draw
}

void CascadedShadowMap::getFrustumCornersWorldSpace(const glm::mat4& viewProj, glm::vec4 corners[8])
{
    const auto inv = glm::inverse(viewProj);

    int idx = 0;
    for (unsigned int x = 0; x < 2; ++x) {
        for (unsigned int y = 0; y < 2; ++y) {
            for (unsigned int z = 0; z < 2; ++z) {
                const glm::vec4 pt = inv * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f);
                corners[idx++] = pt / pt.w;
            }
        }
    }
}

glm::mat4 CascadedShadowMap::getLightSpaceMatrix(const glm::vec3 directionalLightDirection, const CameraProperties& cameraProperties, const float cascadeNear, const float cascadeFar)
{
    const auto proj = glm::perspective(cameraProperties.fov, cameraProperties.aspect, cascadeNear, cascadeFar);

    constexpr int32_t numberOfCorners = 8;
    glm::vec4 corners[numberOfCorners];
    getFrustumCornersWorldSpace(proj, cameraProperties.viewMatrix, corners);

    glm::vec3 center = glm::vec3(0, 0, 0);
    for (const auto& v : corners) {
        center += glm::vec3(v);
    }
    center /= numberOfCorners;

    const auto lightView = glm::lookAt(center + directionalLightDirection, center, glm::vec3(0.0f, 1.0f, 0.0f));

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
    constexpr float zMult = 1 / 10.0f;
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


void CascadedShadowMap::getLightSpaceMatrices(const glm::vec3 directionalLightDirection, const CameraProperties& cameraProperties, glm::mat4 matrices[MAX_SHADOW_CASCADES + 1])
{
    for (size_t i = 0; i < MAX_SHADOW_CASCADES + 1; ++i) {
        if (i == 0) {
            matrices[i] = getLightSpaceMatrix(directionalLightDirection, cameraProperties, cameraProperties.nearPlane, normalizedCascadeLevels[i] * cameraProperties.nearPlane);
        } else if (i < MAX_SHADOW_CASCADES) {
            matrices[i] = getLightSpaceMatrix(directionalLightDirection, cameraProperties, normalizedCascadeLevels[i - 1] * cameraProperties.nearPlane,
                                              normalizedCascadeLevels[i] * cameraProperties.farPlane);
        } else {
            matrices[i] = getLightSpaceMatrix(directionalLightDirection, cameraProperties, normalizedCascadeLevels[i - 1] * cameraProperties.nearPlane, cameraProperties.farPlane);
        }
    }
}
