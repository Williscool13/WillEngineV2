//
// Created by William on 2025-02-23.
//

#ifndef TERRAIN_TYPES_H
#define TERRAIN_TYPES_H
#include <cstdint>
#include <glm/glm.hpp>

namespace will_engine::terrain
{
struct TerrainVertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    int32_t materialIndex;
    glm::vec4 color;
};

struct TerrainConfig
{
    glm::vec2 uvOffset;
    glm::vec2 uvScale;
    glm::vec4 baseColor;
};

struct TerrainProperties
{

    float slopeRockThreshold = 0.7f;
    float slopeRockBlend = 0.2;
    float heightSandThreshold = 0.1f;
    float heightSandBlend = 0.05f;
    float heightGrassThreshold = 0.9f;
    float heightGrassBlend = 0.1f;

    float minHeight = 0.0f;
    float maxHeight = 100.0f;
};
}

#endif //TERRAIN_TYPES_H
