//
// Created by William on 2025-02-24.
//

#ifndef TERRAIN_CHUNK_H
#define TERRAIN_CHUNK_H

#include <array>
#include <glm/detail/type_quat.hpp>

#include "terrain_constants.h"
#include "terrain_types.h"
#include "engine/physics/physics_body.h"
#include "engine/renderer/renderer_constants.h"
#include "engine/renderer/resource_manager.h"
#include "engine/renderer/vk_types.h"
#include "engine/renderer/assets/texture/texture_resource.h"
#include "engine/renderer/resources/buffer.h"

namespace will_engine::terrain
{
class TerrainChunk : public IPhysicsBody
{
public:
    TerrainChunk(renderer::ResourceManager& resourceManager, const std::vector<float>& heightMapData, int32_t width, int32_t height, TerrainConfig terrainConfig);

    ~TerrainChunk() override;

    void generateMesh(int32_t width, int32_t height, const std::vector<float>& heightData);

    static glm::vec3 calculateNormal(int32_t x, int32_t z, int32_t width, int32_t height, const std::vector<float>& heightData);

    static void smoothNormals(std::vector<TerrainVertex>& vertices, int32_t width, int32_t height);

    void update(int32_t currentFrameOverlap, int32_t previousFrameOverlap);

    void setTerrainBufferData(const TerrainProperties& terrainProperties, const std::array<uint32_t, MAX_TERRAIN_TEXTURE_COUNT>& textureIds);

    TerrainProperties getTerrainProperties() const { return terrainProperties; }

    std::array<uint32_t, MAX_TERRAIN_TEXTURE_COUNT> getTerrainTextureIds() const { return textureIds; }

    static std::array<uint32_t, MAX_TERRAIN_TEXTURE_COUNT> getDefaultTextureIds()
    {
        return {DEFAULT_TERRAIN_GRASS_TEXTURE_ID, DEFAULT_TERRAIN_ROCKS_TEXTURE_ID, DEFAULT_TERRAIN_SAND_TEXTURE_ID, 0, 0, 0, 0, 0};
    }

public:
    [[nodiscard]] const VkBuffer getVertexBuffer() const { return vertexBuffer->buffer; }
    [[nodiscard]] const VkBuffer getIndexBuffer() const { return indexBuffer->buffer; }

    [[nodiscard]] size_t getIndexCount() const { return indices.size(); }
    const std::vector<uint32_t>& getIndices() { return indices; }

    [[nodiscard]] renderer::DescriptorBufferSampler* getTextureDescriptorBuffer() const { return textureDescriptorBuffer.get(); }
    [[nodiscard]] renderer::DescriptorBufferUniform* getUniformDescriptorBuffer() const { return uniformDescriptorBuffer.get(); }

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
    renderer::ResourceManager& resourceManager;
    TerrainConfig terrainConfig;

private: // Buffer Data
    TerrainProperties terrainProperties{};
    std::array<uint32_t, MAX_TERRAIN_TEXTURE_COUNT> textureIds{};

private: // Model Data
    std::vector<TerrainVertex> vertices;
    std::vector<uint32_t> indices;

private: // Buffer Data
    renderer::BufferPtr vertexBuffer{};
    renderer::BufferPtr indexBuffer{};

    renderer::DescriptorBufferSamplerPtr textureDescriptorBuffer;
    renderer::DescriptorBufferUniformPtr uniformDescriptorBuffer;

    std::array<renderer::BufferPtr, FRAME_OVERLAP> terrainUniformBuffers{};
    std::vector<std::shared_ptr<renderer::TextureResource> > textureResources{};
    int32_t bufferFramesToUpdate{FRAME_OVERLAP};

private: // Physics
    JPH::BodyID terrainBodyId{JPH::BodyID::cMaxBodyIndex};

    bool bIsPhysicsDirty{true};
};
}

#endif //TERRAIN_CHUNK_H
