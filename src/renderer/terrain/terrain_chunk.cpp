//
// Created by William on 2025-02-24.
//

#include "terrain_chunk.h"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>

#include "src/core/engine.h"

#include "terrain_constants.h"
#include "src/physics/physics.h"
#include "src/physics/physics_filters.h"
#include "src/physics/physics_utils.h"

namespace will_engine::terrain
{
TerrainChunk::TerrainChunk(ResourceManager& resourceManager, const std::vector<float>& heightMapData, int32_t width, int32_t height, TerrainConfig terrainConfig) : resourceManager(resourceManager),
    terrainConfig(terrainConfig)
{
    generateMesh(width, height, heightMapData);

    const size_t vertexBufferSize = vertices.size() * sizeof(TerrainVertex);
    AllocatedBuffer stagingBuffer = resourceManager.createStagingBuffer(vertexBufferSize);
    memcpy(stagingBuffer.info.pMappedData, vertices.data(), vertexBufferSize);

    vertexBuffer = resourceManager.createDeviceBuffer(
        vertexBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );
    resourceManager.copyBuffer(stagingBuffer, vertexBuffer, vertexBufferSize);
    resourceManager.destroyBuffer(stagingBuffer);

    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);
    stagingBuffer = resourceManager.createStagingBuffer(indexBufferSize);
    memcpy(stagingBuffer.info.pMappedData, indices.data(), indexBufferSize);

    indexBuffer = resourceManager.createDeviceBuffer(
        indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT
    );
    resourceManager.copyBuffer(stagingBuffer, indexBuffer, indexBufferSize);
    resourceManager.destroyBuffer(stagingBuffer);

    uploadTextures();

    for (int i{0}; i < FRAME_OVERLAP; i++) {
        terrainUniformBuffers[i] = resourceManager.createHostSequentialBuffer(sizeof(TerrainProperties));
    }
    uniformDescriptorBuffer = resourceManager.createDescriptorBufferUniform(resourceManager.getTerrainUniformLayout(), FRAME_OVERLAP);
    std::vector<DescriptorUniformData> terrainBuffers{1};
    for (int i{0}; i < FRAME_OVERLAP; i++) {
        terrainBuffers[0] = DescriptorUniformData{.uniformBuffer = terrainUniformBuffers[i], .allocSize = sizeof(TerrainProperties)};
        resourceManager.setupDescriptorBufferUniform(uniformDescriptorBuffer, terrainBuffers, i);
    }

    // Physics
    physics::Physics* physics = physics::Physics::get();
    constexpr float halfWidth = static_cast<float>(512 - 1) * 0.5f;
    constexpr float halfHeight = static_cast<float>(512 - 1) * 0.5f;
    JPH::HeightFieldShapeSettings heightFieldSettings{
        heightMapData.data(),
        JPH::Vec3(-halfWidth, 0.0f, -halfHeight),
        JPH::Vec3(1.0f, 1.0f, 1.0f),
        512,
        {},
    };

    physics->setupRigidbody(this, heightFieldSettings, JPH::EMotionType::Static, physics::Layers::TERRAIN);

    TerrainProperties t{};
    setTerrainProperties(t);
}

TerrainChunk::~TerrainChunk()
{
    if (terrainBodyId.GetIndex() != JPH::BodyID::cMaxBodyIndex) {
        if (physics::Physics* physics = physics::Physics::get()) {
            physics->removeRigidBody(this);
            terrainBodyId = JPH::BodyID(JPH::BodyID::cMaxBodyIndex);
        }
    }
    resourceManager.destroyBuffer(vertexBuffer);
    resourceManager.destroyBuffer(indexBuffer);

    for (AllocatedBuffer buffer : terrainUniformBuffers) {
        resourceManager.destroyBuffer(buffer);
    }

    resourceManager.destroyDescriptorBuffer(textureDescriptorBuffer);
    resourceManager.destroyDescriptorBuffer(uniformDescriptorBuffer);
}

void TerrainChunk::generateMesh(const int32_t width, const int32_t height, const std::vector<float>& heightData)
{
    vertices.clear();
    indices.clear();
    vertices.reserve(width * height);

    const float halfWidth = static_cast<float>(width - 1) * 0.5f;
    const float halfHeight = static_cast<float>(height - 1) * 0.5f;

    for (int32_t z = 0; z < height; z++) {
        for (int32_t x = 0; x < width; x++) {
            TerrainVertex vertex{};

            const float xPos = static_cast<float>(x) - halfWidth;
            const float zPos = static_cast<float>(z) - halfHeight;
            const float yPos = heightData[z * width + x];

            vertex.position = glm::vec3(xPos, yPos, zPos);

            const float uvX = static_cast<float>(x) / static_cast<float>(width - 1);
            const float uvY = static_cast<float>(z) / static_cast<float>(height - 1);
            vertex.uv.x = uvX * terrainConfig.uvScale.x + terrainConfig.uvOffset.x;
            vertex.uv.y = uvY * terrainConfig.uvScale.y + terrainConfig.uvOffset.y;

            vertex.materialIndex = 0;

            vertex.color = terrainConfig.baseColor;

            vertices.push_back(vertex);
        }
    }

    for (int32_t z = 0; z < height; z++) {
        for (int32_t x = 0; x < width; x++) {
            vertices[z * width + x].normal = calculateNormal(x, z, width, height, heightData);
        }
    }

    smoothNormals(vertices, width, height);

    // Quads
    for (uint32_t z = 0; z < height - 1; z++) {
        for (uint32_t x = 0; x < width - 1; x++) {
            // Top-left
            indices.push_back(z * width + x);
            // Top-right
            indices.push_back(z * width + x + 1);
            // Bottom-left
            indices.push_back((z + 1) * width + x);
            // Bottom-right
            indices.push_back((z + 1) * width + x + 1);
        }
    }
}

glm::vec3 TerrainChunk::calculateNormal(const int32_t x, const int32_t z, const int32_t width, const int32_t height, const std::vector<float>& heightData)
{
    const int32_t xLeft = glm::max(0, x - 1);
    const int32_t xRight = glm::min(width - 1, x + 1);
    const int32_t zTop = glm::max(0, z - 1);
    const int32_t zBottom = glm::min(height - 1, z + 1);

    const float hL = heightData[z * width + xLeft];
    const float hR = heightData[z * width + xRight];
    const float hT = heightData[zTop * width + x];
    const float hB = heightData[zBottom * width + x];

    const float hTL = heightData[zTop * width + xLeft];
    const float hTR = heightData[zTop * width + xRight];
    const float hBL = heightData[zBottom * width + xLeft];
    const float hBR = heightData[zBottom * width + xRight];

    const float dX = (hR - hL) * 2.0f + (hTR - hTL) + (hBR - hBL);
    const float dZ = (hB - hT) * 2.0f + (hBL - hTL) + (hBR - hTR);

    const float scale = 1.0f / (4.0f * (xRight - xLeft));

    const glm::vec3 normal(-dX * scale, 1.0f, -dZ * scale);
    return glm::normalize(normal);
}

void TerrainChunk::smoothNormals(std::vector<TerrainVertex>& vertices, const int32_t width, const int32_t height)
{
    std::vector<glm::vec3> smoothedNormals(width * height);

    for (int32_t z = 0; z < height; z++) {
        for (int32_t x = 0; x < width; x++) {
            glm::vec3 avgNormal(0.0f);
            int count = 0;

            for (int32_t nz = glm::max(0, z - 1); nz <= glm::min(height - 1, z + 1); nz++) {
                for (int32_t nx = glm::max(0, x - 1); nx <= glm::min(width - 1, x + 1); nx++) {
                    avgNormal += vertices[nz * width + nx].normal;
                    count++;
                }
            }

            if (count > 0) {
                avgNormal /= static_cast<float>(count);
                smoothedNormals[z * width + x] = normalize(avgNormal);
            }
        }
    }

    for (int32_t i = 0; i < width * height; i++) {
        vertices[i].normal = smoothedNormals[i];
    }
}

void TerrainChunk::uploadTextures()
{
    std::vector<DescriptorImageData> textureDescriptors;
    constexpr std::array defaultTextures{DEFAULT_TERRAIN_GRASS_TEXTURE_ID, DEFAULT_TERRAIN_ROCKS_TEXTURE_ID, DEFAULT_TERRAIN_SAND_TEXTURE_ID};
    textureDescriptors.reserve(defaultTextures.size());

    for (const uint32_t textureId : defaultTextures) {
        if (Texture* texture = Engine::get()->getAssetManager()->getTexture(textureId)) {
            texture->load();
            textureDescriptors.push_back({
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                {
                    .sampler = resourceManager.getDefaultSamplerMipMappedNearest(),
                    .imageView = texture->getTextureResource().imageView,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                },
                false
            });
        }
        else {
            textureDescriptors.push_back({
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, {}, true
            });
        }
    }

    textureDescriptorBuffer = resourceManager.createDescriptorBufferSampler(resourceManager.getTerrainTexturesLayout(), 1);
    resourceManager.setupDescriptorBufferSampler(textureDescriptorBuffer, textureDescriptors, 0);
}

void TerrainChunk::update(const int32_t currentFrameOverlap, const int32_t previousFrameOverlap)
{
    if (bufferFramesToUpdate <= 0) { return; }

    const AllocatedBuffer& currentFrameUniformBuffer = terrainUniformBuffers[currentFrameOverlap];
    const auto pUniformBuffer = reinterpret_cast<TerrainProperties*>(static_cast<char*>(currentFrameUniformBuffer.info.pMappedData));
    memcpy(pUniformBuffer, &terrainProperties, sizeof(TerrainProperties));

    bufferFramesToUpdate--;
}

void TerrainChunk::setTerrainProperties(TerrainProperties newTerrainProperties)
{
    terrainProperties = newTerrainProperties;
    bufferFramesToUpdate = FRAME_OVERLAP;
}
}
