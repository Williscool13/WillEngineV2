//
// Created by William on 2025-03-09.
//

#ifndef TERRAIN_H
#define TERRAIN_H
#include "src/renderer/terrain/terrain_chunk.h"

namespace will_engine
{
class ITerrain
{
public:
    virtual ~ITerrain() = default;

    virtual terrain::TerrainChunk* getTerrainChunk() = 0;
};
}

#endif //TERRAIN_H
