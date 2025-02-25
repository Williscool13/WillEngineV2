//
// Created by William on 2025-02-24.
//

#ifndef TERRAIN_CHUNK_H
#define TERRAIN_CHUNK_H

#include "terrain_types.h"
#include "src/renderer/resource_manager.h"
#include "src/renderer/vk_types.h"

namespace will_engine::terrain
{
class TerrainChunk
{
public:
    TerrainChunk(ResourceManager& resourceManager, const std::vector<float>& heightMapData, int32_t width, int32_t height);

    ~TerrainChunk();

    void generateMesh(int32_t width, int32_t height, const std::vector<float>& heightData);

    static glm::vec3 calculateNormal(int32_t x, int32_t z, int32_t width, int32_t height, const std::vector<float>& heightData);

public:
    const std::vector<uint32_t>& getIndices() { return indices; }

private:
    ResourceManager& resourceManager;

private: // Model Data
    std::vector<TerrainVertex> vertices;
    std::vector<uint32_t> indices;

private: // Buffer Data
    AllocatedBuffer vertexBuffer{};
    AllocatedBuffer indexBuffer{};
    //AllocatedBuffer instanceBuffer{};
    //AllocatedBuffer materialBuffer{};
};
}

#endif //TERRAIN_CHUNK_H
