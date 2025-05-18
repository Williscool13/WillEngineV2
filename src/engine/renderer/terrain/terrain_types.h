//
// Created by William on 2025-02-23.
//

#ifndef TERRAIN_TYPES_H
#define TERRAIN_TYPES_H
#include <cstdint>
#include <glm/glm.hpp>
#include <json/json.hpp>

namespace will_engine::terrain
{
using ordered_json = nlohmann::ordered_json;


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

inline void to_json(ordered_json& j, const TerrainConfig& config)
{
    j = {
        {
            "uvOffset", {
                {"x", config.uvOffset.x},
                {"y", config.uvOffset.y}
            }
        },
        {
            "uvScale", {
                {"x", config.uvScale.x},
                {"y", config.uvScale.y},
            }
        },
        {
            "baseColor", {
                {"x", config.baseColor.x},
                {"y", config.baseColor.y},
                {"z", config.baseColor.z},
                {"w", config.baseColor.w},
            }
        }
    };
}

inline void from_json(const ordered_json& j, TerrainConfig& config)
{
    config.uvOffset.x = j["uvOffset"]["x"].get<float>();
    config.uvOffset.y = j["uvOffset"]["y"].get<float>();
    config.uvScale.x = j["uvScale"]["x"].get<float>();
    config.uvScale.y = j["uvScale"]["y"].get<float>();

    config.baseColor.x = j["baseColor"]["x"].get<float>();
    config.baseColor.y = j["baseColor"]["y"].get<float>();
    config.baseColor.z = j["baseColor"]["z"].get<float>();
    config.baseColor.w = j["baseColor"]["w"].get<float>();
}

inline void to_json(ordered_json& j, const TerrainProperties& properties)
{
    j = {
        {"slopeRockThreshold", properties.slopeRockThreshold},
        {"slopeRockBlend", properties.slopeRockBlend},
        {"heightSandThreshold", properties.heightSandThreshold},
        {"heightSandBlend", properties.heightSandBlend},
        {"heightGrassThreshold", properties.heightGrassThreshold},
        {"heightGrassBlend", properties.heightGrassBlend},
        {"minHeight", properties.minHeight},
        {"maxHeight", properties.maxHeight}
    };
}

inline void from_json(const ordered_json& j, TerrainProperties& properties)
{
    properties.slopeRockThreshold = j["slopeRockThreshold"].get<float>();
    properties.slopeRockBlend = j["slopeRockBlend"].get<float>();
    properties.heightSandThreshold = j["heightSandThreshold"].get<float>();
    properties.heightSandBlend = j["heightSandBlend"].get<float>();
    properties.heightGrassThreshold = j["heightGrassThreshold"].get<float>();
    properties.heightGrassBlend = j["heightGrassBlend"].get<float>();
    properties.minHeight = j["minHeight"].get<float>();
    properties.maxHeight = j["maxHeight"].get<float>();
}
}

#endif //TERRAIN_TYPES_H
