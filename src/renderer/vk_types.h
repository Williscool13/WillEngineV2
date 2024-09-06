//
// Created by William on 8/11/2024.
//

#ifndef VKTYPES_H
#define VKTYPES_H

#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>

struct CubemapImageView {
    VkImageView imageView;
    VkExtent3D imageExtent;
    float roughness;
    int descriptorBufferIndex;
};

struct AllocatedImage {
    VkImage image {VK_NULL_HANDLE};
    VkImageView imageView{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
    VkExtent3D imageExtent{};
    VkFormat imageFormat{};
};

struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct AllocatedCubemap {
    AllocatedImage allocatedImage;
    std::vector<CubemapImageView> cubemapImageViews;
    int mipLevels; //should be equal to cubemapImageViews.size()

};


struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal{1.0f, 0.0f, 0.0f};
    glm::vec4 color{1.0f};
    glm::vec2 uv{0,0};
    glm::uint32_t materialIndex{0};

};

struct Mesh
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

struct Material
{
    glm::vec4 colorFactor{1.0f};
    glm::vec4 metalRoughFactors{0.0f, 1.0f, 0.0f, 0.0f}; // x: metallic, y: roughness
    glm::vec4 textureImageIndices{-1.0f};   // x: base color, y: metallic roughness, z: pad, w: pad
    glm::vec4 textureSamplerIndices{-1.0f}; // x: base color, y: metallic roughness, z: pad, w: pad
    glm::vec4 alphaCutoff{1.0f, 0.0f, 0.0f, 0.0f}; // x: alpha cutoff, y: alpha mode, z: padding, w: padding
};

enum class MaterialType
{
    OPAQUE = 0,
    TRANSPARENT = 1,
    MASK = 2,
};

#endif //VKTYPES_H
