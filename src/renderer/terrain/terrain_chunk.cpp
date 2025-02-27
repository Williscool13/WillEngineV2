//
// Created by William on 2025-02-24.
//

#include "terrain_chunk.h"

namespace will_engine::terrain
{
TerrainChunk::TerrainChunk(ResourceManager& resourceManager, const std::vector<float>& heightMapData, int32_t width, int32_t height) : resourceManager(resourceManager)
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

    //instanceBuffer = resourceManager.createHostSequentialBuffer(sizeof(TerrainInstanceData));

    //setupTerrainTextures(resourceManager);
}

TerrainChunk::~TerrainChunk()
{
    resourceManager.destroyBuffer(vertexBuffer);
    resourceManager.destroyBuffer(indexBuffer);
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

            vertex.uv.x = static_cast<float>(x) / static_cast<float>(width - 1);
            vertex.uv.y = static_cast<float>(z) / static_cast<float>(height - 1);

            vertices.push_back(vertex);
        }
    }

    for (int32_t z = 0; z < height; z++) {
        for (int32_t x = 0; x < width; x++) {
            vertices[z * width + x].normal = calculateNormal(x, z, width, height, heightData);
        }
    }

    constexpr uint32_t PRIMITIVE_RESTART_INDEX = 0xFFFFFFFF;

    // Triangle Strips
    for (uint32_t z = 0; z < height - 1; z++) {
        for (uint32_t x = 0; x < width; x++) {
            indices.push_back(z * width + x);
            indices.push_back((z + 1) * width + x);

        }
        // Second to last row
        if (z == height - 2) { break; }
        indices.push_back(PRIMITIVE_RESTART_INDEX);
    }
}

glm::vec3 TerrainChunk::calculateNormal(const int32_t x, const int32_t z, const int32_t width, const int32_t height, const std::vector<float>& heightData)
{
    // Handle edges
    const int32_t xLeft = glm::max(0, x - 1);
    const int32_t xRight = glm::min(width - 1, x + 1);
    const int32_t zTop = glm::max(0, z - 1);
    const int32_t zBottom = glm::min(height - 1, z + 1);

    // Get heights of surrounding vertices
    const float hL = heightData[z * width + xLeft];
    const float hR = heightData[z * width + xRight];
    const float hT = heightData[zTop * width + x];
    const float hB = heightData[zBottom * width + x];

    // Calculate partial derivatives
    const float dX = (hR - hL) / static_cast<float>(xRight - xLeft);
    const float dZ = (hB - hT) / static_cast<float>(zBottom - zTop);

    // Normal is cross product of tangent vectors (1,dX,0) and (0,dZ,1)
    const glm::vec3 normal(-dX, 1.0f, -dZ);
    return normalize(normal);
}
}
