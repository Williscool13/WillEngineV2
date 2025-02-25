//
// Created by William on 2025-02-23.
//

#ifndef TERRAIN_TYPES_H
#define TERRAIN_TYPES_H
#include <cstdint>
#include <glm/glm.hpp>

namespace will_engine::terrain
{
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

struct TerrainVertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};
}

#endif //TERRAIN_TYPES_H
