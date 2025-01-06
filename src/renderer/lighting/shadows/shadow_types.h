//
// Created by William on 2025-01-06.
//

#ifndef SHADOW_TYPES_H
#define SHADOW_TYPES_H

#include <glm/glm.hpp>

struct ShadowMapPipelineCreateInfo
{
    VkDescriptorSetLayout modelAddressesLayout;
    VkDescriptorSetLayout shadowMapLayout;
};

struct ShadowMapPushConstants
{
    glm::mat4 lightMatrix{};
};

#endif //SHADOW_TYPES_H
