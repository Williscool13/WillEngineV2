//
// Created by William on 2025-01-24.
//

#ifndef MODEL_TYPES_H
#define MODEL_TYPES_H

#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

#include "engine/core/transform.h"

namespace will_engine
{
enum class MaterialType
{
    OPAQUE      = 0,
    TRANSPARENT = 1,
    MASK        = 2,
};

struct MaterialProperties
{
    glm::vec4 colorFactor{1.0f};
    glm::vec4 metalRoughFactors{0.0f, 1.0f, 0.0f, 0.0f}; // x: metallic, y: roughness
    glm::ivec4 textureImageIndices{-1}; // x: color image, y: metallic image, z: pad, w: pad
    glm::ivec4 textureSamplerIndices{-1}; // x: color sampler, y: metallic sampler, z: pad, w: pad
    glm::vec4 alphaCutoff{1.0f, 0.0f, 0.0f, 0.0f}; // x: alpha cutoff, y: alpha mode, z: padding, w: padding
};

struct VertexPosition
{
    glm::vec3 position{0.0f};
};

struct VertexProperty
{
    glm::vec3 normal{1.0f, 0.0f, 0.0f};
    glm::vec4 color{1.0f};
    glm::vec2 uv{0,0};
};

struct Primitive
{
    uint32_t firstIndex{0}; // ID of the instance (used to get model matrix)
    uint32_t indexCount{0};
    int32_t vertexOffset{0};
    bool bHasTransparent{false};
    uint32_t boundingSphereIndex{0};
    uint32_t materialIndex{0};
};

struct Mesh
{
    std::string name;
    std::vector<Primitive> primitives;
};

struct BoundingSphere {
    BoundingSphere() = default;
    explicit BoundingSphere(const std::vector<VertexPosition>& vertices);

    glm::vec3 center{};
    float radius{};
};

struct RenderNode
{
    std::string name;
    Transform transform;
    std::vector<RenderNode*> children;
    RenderNode* parent;
    int32_t meshIndex{-1};
};

struct PrimitiveData
{
    uint32_t materialIndex;
    uint32_t instanceDataIndex;
    uint32_t boundingVolumeIndex;
    uint32_t bHasTransparent;
};

struct InstanceData
{
    glm::mat4 currentModelMatrix;
    glm::mat4 previousModelMatrix;
    glm::vec4 flags; // x: visible, y: casts shadows, z,w: reserved for future use
};

struct FrustumCullingBuffers
{
    VkDeviceAddress meshBoundsBuffer;
    VkDeviceAddress commandBuffer;
    uint32_t commandBufferCount;
    glm::vec3 padding;
};

}


#endif //MODEL_TYPES_H
