//
// Created by William on 8/11/2024.
//

#ifndef VKTYPES_H
#define VKTYPES_H

#include <vector>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>

struct AllocatedImage
{
    VkImage image{VK_NULL_HANDLE};
    VkImageView imageView{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
    VkExtent3D imageExtent{};
    VkFormat imageFormat{};
};

struct CubemapImageView
{
    VkImageView imageView;
    VkExtent3D imageExtent;
    float roughness;
    int32_t descriptorBufferIndex;
};

struct AllocatedCubemap
{
    AllocatedImage allocatedImage;
    std::vector<CubemapImageView> cubemapImageViews; // one for each active mip level
};

struct AllocatedBuffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct SceneData
{
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
    glm::mat4 viewProj{1.0f};

    glm::mat4 invView{1.0f};
    glm::mat4 invProj{1.0f};
    glm::mat4 invViewProj{1.0f};

    glm::mat4 viewProjCameraLookDirection{1.0f};

    glm::mat4 prevView{1.0f};
    glm::mat4 prevProj{1.0f};
    glm::mat4 prevViewProj{1.0f};

    glm::mat4 prevInvView{1.0f};
    glm::mat4 prevInvProj{1.0f};
    glm::mat4 prevInvViewProj{1.0f};

    glm::mat4 prevViewProjCameraLookDirection{1.0f};

    glm::vec4 cameraWorldPos{0.0f};
    glm::vec4 prevCameraWorldPos{0.0f};

    /**
     * x,y is current; z,w is previous
     */
    glm::vec4 jitter{0.0f};

    glm::vec2 renderTargetSize{};
    float deltaTime{};
};

#endif //VKTYPES_H
