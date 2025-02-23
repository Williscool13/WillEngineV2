//
// Created by William on 2025-02-23.
//

#ifndef TERRAIN_TYPES_H
#define TERRAIN_TYPES_H
#include <cstdint>


struct TerrainConfig
{
    uint32_t width = 128;
    uint32_t height = 128;
    float scale = 100.0f;
    float heightScale = 20.0f;
};

struct TerrainRenderData
{
    uint32_t renderObjectId;
    uint32_t materialId;
};


#endif //TERRAIN_TYPES_H
