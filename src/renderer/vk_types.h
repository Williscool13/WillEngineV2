//
// Created by William on 8/11/2024.
//

#ifndef VKTYPES_H
#define VKTYPES_H

#include <vector>
#include <string>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>


enum class MaterialType
{
    OPAQUE      = 0,
    TRANSPARENT = 1,
    MASK        = 2,
};

enum class LightType
{
    DIRECTIONAL = 0,
    POINT       = 1,
    SPOT        = 2,
    // AREA = 3,
};

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
    VkBuffer buffer{VK_NULL_HANDLE};
    VmaAllocation allocation{};
    VmaAllocationInfo info{};
};

struct AllocatedCubemap {
    AllocatedImage allocatedImage;
    std::vector<CubemapImageView> cubemapImageViews;
    int mipLevels; //should be equal to cubemapImageViews.size()
};


struct SceneData
{
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
    glm::mat4 viewProj{1.0f};
    glm::vec4 cameraWorldPos{0.0f};
    glm::mat4 viewProjCameraLookDirection{1.0f};

    glm::mat4 invView{1.0f};
    glm::mat4 invProj{1.0f};
    glm::mat4 invViewProj{1.0f};

    glm::mat4 prevView;
    glm::mat4 prevProj;
    glm::mat4 prevViewProj;

    glm::vec4 jitter;

    glm::vec2 renderTargetSize;
    int32_t frameNumber; // either 0 or 1
    float deltaTime;
};

struct DeferredResolveData
{
    uint32_t width{1700};
    uint32_t height{900};
    uint32_t debug{0};
};


struct LightData
{

};

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal{1.0f, 0.0f, 0.0f};
    glm::vec4 color{1.0f};
    glm::vec2 uv{0,0};
    glm::uint32_t materialIndex{0};
};

struct Primitive
{
    uint32_t firstIndex{0}; // ID of the instance (used to get model matrix)
    uint32_t indexCount{0};
    int32_t vertexOffset{0};
    bool hasTransparents{false};
    uint32_t boundingSphereIndex{0};
};

struct Mesh
{
    std::string name;
    std::vector<Primitive> primitives;
};

struct Material
{
    glm::vec4 colorFactor{1.0f};
    glm::vec4 metalRoughFactors{0.0f, 1.0f, 0.0f, 0.0f}; // x: metallic, y: roughness
    glm::ivec4 textureImageIndices{-1};   // x: color image, y: metallic image, z: pad, w: pad
    glm::ivec4 textureSamplerIndices{-1}; // x: color sampler, y: metallic sampler, z: pad, w: pad
    glm::vec4 alphaCutoff{1.0f, 0.0f, 0.0f, 0.0f}; // x: alpha cutoff, y: alpha mode, z: padding, w: padding
};

struct InstanceData
{
    glm::mat4 currentModelMatrix;
    glm::mat4 previousModelMatrix;
};

struct DrawIndirectData
{
    VkDrawIndexedIndirectCommand indirectCommand{};
    uint32_t boundingSphereIndex{0};
};

/**
 * commandbuffer, modelMatrix, and meshIndices should be of equal length at all times.
 */
struct FrustumCullingBuffers
{
    VkDeviceAddress meshBoundsBuffer;
    VkDeviceAddress commandBuffer;
    uint32_t commandBufferCount;
    VkDeviceAddress modelMatrixBuffer;
    VkDeviceAddress meshIndicesBuffer;
    glm::vec3 padding;
};

struct TaaProperties
{
    glm::vec2 texelSize;
    int32_t width;
    int32_t height;
    /**
     * the minimum amount of history to be used for TAA. Lower means less of the history will be used at lower confidence
     */
    float minBlend;
    /**
     * the maximum amount of history to be used for TAA
     */
    float maxBlend;
    float velocityWeight;
    float zVelocity;
    bool bEnabled;
    int32_t taaDebug;
};

struct PostProcessData
{
    int32_t width;
    int32_t height;
    bool bEnable;
    int32_t padding;
};

#endif //VKTYPES_H
