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
}

#endif //TERRAIN_TYPES_H
