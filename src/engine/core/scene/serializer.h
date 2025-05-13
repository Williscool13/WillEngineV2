//
// Created by William on 2025-02-06.
//

#ifndef SCENE_SERIALIZER_H
#define SCENE_SERIALIZER_H

#include <json/json.hpp>


#include "map.h"
#include "serializer_types.h"
#include "engine/core/transform.h"
#include "engine/core/game_object/game_object.h"
#include "engine/core/game_object/components/component.h"
#include "engine/core/game_object/components/component_factory.h"
#include "engine/renderer/assets/render_object/render_object.h"
#include "engine/renderer/assets/texture/texture.h"


namespace will_engine
{
class Engine;
using ordered_json = nlohmann::ordered_json;
using json = nlohmann::json;

inline void to_json(ordered_json& j, const glm::vec2& v)
{
    j = {
        {"x", v.x},
        {"y", v.y}
    };
}

inline void from_json(const ordered_json& j, glm::vec2& v)
{
    v.x = j["x"].get<float>();
    v.y = j["y"].get<float>();
}

inline void to_json(ordered_json& j, const glm::vec3& v)
{
    j = {
        {"x", v.x},
        {"y", v.y},
        {"z", v.z}
    };
}

inline void from_json(const ordered_json& j, glm::vec3& v)
{
    v.x = j["x"].get<float>();
    v.y = j["y"].get<float>();
    v.z = j["z"].get<float>();
}

inline void to_json(ordered_json& j, const glm::vec4& v)
{
    j = {
        {"x", v.x},
        {"y", v.y},
        {"z", v.z},
        {"w", v.w}
    };
}

inline void from_json(const ordered_json& j, glm::vec4& v)
{
    v.x = j["x"].get<float>();
    v.y = j["y"].get<float>();
    v.z = j["z"].get<float>();
    v.w = j["w"].get<float>();
}

inline void to_json(ordered_json& j, const NoiseSettings& settings)
{
    j = {
        {"scale", settings.scale},
        {"persistence", settings.persistence},
        {"lacunarity", settings.lacunarity},
        {"octaves", settings.octaves},
        {
            "offset", {
                {"x", settings.offset.x},
                {"y", settings.offset.y}
            }
        },
        {"heightScale", settings.heightScale}
    };
}

inline void from_json(const ordered_json& j, NoiseSettings& settings)
{
    settings.scale = j["scale"].get<float>();
    settings.persistence = j["persistence"].get<float>();
    settings.lacunarity = j["lacunarity"].get<float>();
    settings.octaves = j["octaves"].get<int>();

    glm::vec2 offset{
        j["offset"]["x"].get<float>(),
        j["offset"]["y"].get<float>()
    };
    settings.offset = offset;

    settings.heightScale = j["heightScale"].get<float>();
}


inline void to_json(ordered_json& j, const Transform& t)
{
    j = {
        {
            "position", {
                {"x", t.getPosition().x},
                {"y", t.getPosition().y},
                {"z", t.getPosition().z}
            }
        },
        {
            "rotation", {
                {"x", t.getRotation().x},
                {"y", t.getRotation().y},
                {"z", t.getRotation().z},
                {"w", t.getRotation().w}
            }
        },
        {
            "scale", {
                {"x", t.getScale().x},
                {"y", t.getScale().y},
                {"z", t.getScale().z}
            }
        }

    };
}

inline void from_json(const ordered_json& j, Transform& t)
{
    glm::vec3 position{
        j["position"]["x"].get<float>(),
        j["position"]["y"].get<float>(),
        j["position"]["z"].get<float>()
    };

    glm::quat rotation{
        j["rotation"]["w"].get<float>(),
        j["rotation"]["x"].get<float>(),
        j["rotation"]["y"].get<float>(),
        j["rotation"]["z"].get<float>()
    };

    glm::vec3 scale{
        j["scale"]["x"].get<float>(),
        j["scale"]["y"].get<float>(),
        j["scale"]["z"].get<float>()
    };

    t = {position, rotation, scale};
}

inline void to_json(ordered_json& j, const EngineVersion& version)
{
    j = ordered_json{
        {"major", version.major},
        {"minor", version.minor},
        {"patch", version.patch}
    };
}

inline void from_json(const ordered_json& j, EngineVersion& version)
{
    version.major = j["major"].get<uint32_t>();
    version.minor = j["minor"].get<uint32_t>();
    version.patch = j["patch"].get<uint32_t>();
}

inline void to_json(ordered_json& j, const SceneMetadata& metadata)
{
    j = ordered_json{
        {"name", metadata.name},
        {"created", metadata.created},
        {"formatVersion", metadata.formatVersion}
    };
}

inline void from_json(const ordered_json& j, SceneMetadata& metadata)
{
    metadata.name = j["name"].get<std::string>();
    metadata.created = j["created"].get<std::string>();
    metadata.formatVersion = j["formatVersion"].get<uint32_t>();
}

inline void to_json(json& j, const TextureProperties& t)
{
    j = ordered_json{
        {
            "mipmapped", t.mipmapped
        }
    };
}

inline void from_json(const json& j, TextureProperties& t)
{
    t = {j["mipmapped"].get<bool>()};
}

enum class EngineSettingsTypeFlag : uint32_t
{
    NONE = 0,
    GENERAL_SETTINGS = 1 << 0,
    CAMERA_SETTINGS = 1 << 1,
    LIGHT_SETTINGS = 1 << 2,
    ENVIRONMENT_SETTINGS = 1 << 3,
    RENDERER_SETTINGS = 1 << 4,
    AMBIENT_OCCLUSION_SETTINGS = 1 << 5,
    SCREEN_SPACE_SHADOWS_SETTINGS = 1 << 6,
    CASCADED_SHADOW_MAP_SETTINGS = 1 << 7,
    TEMPORAL_ANTIALIASING_SETTINGS = 1 << 8,
    POSTPROCESS_SETTINGS = 1 << 9,
    ALL_SETTINGS = 0xFFFFFFFF
};

inline EngineSettingsTypeFlag operator|(EngineSettingsTypeFlag a, EngineSettingsTypeFlag b)
{
    return static_cast<EngineSettingsTypeFlag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline EngineSettingsTypeFlag operator&(EngineSettingsTypeFlag a, EngineSettingsTypeFlag b)
{
    return static_cast<EngineSettingsTypeFlag>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline EngineSettingsTypeFlag& operator|=(EngineSettingsTypeFlag& a, EngineSettingsTypeFlag b)
{
    return a = a | b;
}

inline EngineSettingsTypeFlag& operator&=(EngineSettingsTypeFlag& a, EngineSettingsTypeFlag b)
{
    return a = a & b;
}


inline bool hasFlag(EngineSettingsTypeFlag flags, EngineSettingsTypeFlag flag)
{
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) == static_cast<uint32_t>(flag);
}

class Serializer
{
public: // GameObjects
    static void serializeGameObject(ordered_json& j, IHierarchical* obj);

    static game_object::GameObject* deserializeGameObject(const ordered_json& j, IHierarchical* parent);

    static bool serializeMap(IHierarchical* map, ordered_json& rootJ, const std::filesystem::path& filepath);

    static bool deserializeMap(IHierarchical* root, ordered_json& rootJ);

public: // Render Objects
    static bool generateWillModel(const std::filesystem::path& gltfPath, const std::filesystem::path& outputPath);

    static std::optional<RenderObjectInfo> loadWillModel(const std::filesystem::path& willmodelPath);

public: // Textures
    static bool generateWillTexture(const std::filesystem::path& texturePath, const std::filesystem::path& outputPath);

    static std::optional<TextureInfo> loadWillTexture(const std::filesystem::path& willtexturePath);

public: // Engine Settings
    static bool serializeEngineSettings(Engine* engine, EngineSettingsTypeFlag engineSettings = EngineSettingsTypeFlag::ALL_SETTINGS);

    static bool deserializeEngineSettings(Engine* engine, EngineSettingsTypeFlag engineSettings = EngineSettingsTypeFlag::ALL_SETTINGS);

public: //
    static uint32_t computePathHash(const std::filesystem::path& path)
    {
        const std::string normalizedPath = path.lexically_normal().string();
        return std::hash<std::string>{}(normalizedPath);
    }
};
} // will_engine

#endif //SCENE_SERIALIZER_H
