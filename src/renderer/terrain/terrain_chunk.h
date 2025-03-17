//
// Created by William on 2025-02-24.
//

#ifndef TERRAIN_CHUNK_H
#define TERRAIN_CHUNK_H

#include <glm/detail/type_quat.hpp>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include "terrain_types.h"
#include "src/physics/physics_body.h"
#include "src/renderer/resource_manager.h"
#include "src/renderer/vk_types.h"

namespace will_engine::terrain
{
class TerrainChunk : public IPhysicsBody
{
public:
    TerrainChunk(ResourceManager& resourceManager, const std::vector<float>& heightMapData, int32_t width, int32_t height);

    ~TerrainChunk() override;

    void generateMesh(int32_t width, int32_t height, const std::vector<float>& heightData);

    static glm::vec3 calculateNormal(int32_t x, int32_t z, int32_t width, int32_t height, const std::vector<float>& heightData);

    static void smoothNormals(std::vector<TerrainVertex>& vertices, int32_t width, int32_t height);

public:
    [[nodiscard]] const AllocatedBuffer& getVertexBuffer() const { return vertexBuffer; }
    [[nodiscard]] const AllocatedBuffer& getIndexBuffer() const { return indexBuffer; }

    [[nodiscard]] size_t getIndexCount() const { return indices.size(); }
    const std::vector<uint32_t>& getIndices() { return indices; }

    [[nodiscard]] const DescriptorBufferSampler& getTextureDescriptorBuffer() const { return textureDescriptorBuffer; }

public: // Physics
    void setTransform(const glm::vec3& position, const glm::quat& rotation) override {}

    glm::vec3 getGlobalPosition() override { return glm::vec3(0.0f); }

    glm::quat getGlobalRotation() override { return glm::quat(1.0f, 0.0f, 0.0f, 0.0f); }

    void setPhysicsBodyId(const JPH::BodyID bodyId) override { terrainBodyId = bodyId; }

    [[nodiscard]] JPH::BodyID getPhysicsBodyId() const override { return terrainBodyId; }

    void dirty() override { bIsPhysicsDirty = true; }

    void undirty() override { bIsPhysicsDirty = false; }

    bool isTransformDirty() override { return bIsPhysicsDirty; }

private:
    ResourceManager& resourceManager;

private: // Model Data
    std::vector<TerrainVertex> vertices;
    std::vector<uint32_t> indices;

private: // Buffer Data
    AllocatedBuffer vertexBuffer{};
    AllocatedBuffer indexBuffer{};

    DescriptorBufferSampler textureDescriptorBuffer;

private: // Physics
    JPH::BodyID terrainBodyId{JPH::BodyID::cMaxBodyIndex};

    bool bIsPhysicsDirty{true};
};
}

#endif //TERRAIN_CHUNK_H
