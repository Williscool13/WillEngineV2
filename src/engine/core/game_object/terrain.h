//
// Created by William on 2025-03-09.
//

#ifndef TERRAIN_H
#define TERRAIN_H
#include "engine/renderer/terrain/terrain_chunk.h"

namespace will_engine
{
class ITerrain
{
public:
    virtual ~ITerrain() = default;

    virtual terrain::TerrainChunk* getTerrainChunk() = 0;

    virtual void generateTerrain() = 0;

    virtual void generateTerrain(terrain::TerrainProperties terrainProperties, std::array<uint32_t, terrain::MAX_TERRAIN_TEXTURE_COUNT> textures) = 0;

    virtual void destroyTerrain() = 0;
};
}

#endif //TERRAIN_H
