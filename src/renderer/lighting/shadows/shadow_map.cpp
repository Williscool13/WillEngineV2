//
// Created by William on 2025-01-01.
//

#include "shadow_map.h"

#include "shadow_map_descriptor_layouts.h"
#include "src/renderer/resource_manager.h"

ShadowMap::ShadowMap(const VulkanContext& context, ResourceManager& resourceManager, ShadowMapDescriptorLayouts& shadowMapDescriptorLayouts) : context(context), resourceManager(resourceManager),
    descriptorLayouts(shadowMapDescriptorLayouts)
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


        for (AllocatedImage& shadowMap : shadowMaps) {
            shadowMap = resourceManager.createImage({extent.width, extent.height, 1}, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        }
    }

    // Create Pipelines
    {

    }

    // Create Descriptor Buffer
    {
        std::vector<DescriptorImageData> textureDescriptors;
        for (AllocatedImage& shadowMap : shadowMaps) {
            VkDescriptorImageInfo imageInfo{.imageView = shadowMap.imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            textureDescriptors.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfo, false});
        }
        shadowMapDescriptorBuffer = DescriptorBufferSampler(context, shadowMapDescriptorLayouts.getShadowMapLayout(), 1);
        shadowMapDescriptorBuffer.setupData(context.device, textureDescriptors);
    }
}

ShadowMap::~ShadowMap()
{
    shadowMapDescriptorBuffer.destroy(context.allocator);
}

void ShadowMap::getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view, glm::vec4 corners[8])
{
    const auto inv = glm::inverse(proj * view);

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
