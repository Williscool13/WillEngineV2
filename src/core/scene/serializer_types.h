//
// Created by William on 2025-03-17.
//

#ifndef SERIALIZER_TYPES_H
#define SERIALIZER_TYPES_H
#include <filesystem>
#include <string>

#include "serializer_constants.h"
#include "src/renderer/assets/texture/texture.h"

namespace will_engine
{
struct EngineVersion
{
    uint32_t major;
    uint32_t minor;
    uint32_t patch;

    static EngineVersion current()
    {
        return {ENGINE_VERSION_MAJOR, ENGINE_VERSION_MINOR, ENGINE_VERSION_PATCH};
    }

    bool operator==(const EngineVersion& other) const
    {
        return major == other.major && minor == other.minor && patch == other.patch;
    }

    bool operator<(const EngineVersion& other) const
    {
        if (major != other.major) return major < other.major;
        if (minor != other.minor) return minor < other.minor;
        return patch < other.patch;
    }

    bool operator>(const EngineVersion& other) const
    {
        return other < *this;
    }

    std::string toString() const
    {
        return fmt::format("{}.{}.{}", major, minor, patch);
    }
};

struct SceneMetadata
{
    std::string name;
    std::string created;
    uint32_t formatVersion;

    static SceneMetadata create(const std::string& sceneName)
    {
        const auto now = std::chrono::system_clock::now();
        const auto timeT = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        tm timeInfo;
        localtime_s(&timeInfo, &timeT);
        ss << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S");

        return {
            sceneName,
            ss.str(),
            SCENE_FORMAT_VERSION
        };
    }
};

struct RenderObjectInfo
{
    std::filesystem::path willmodelPath;
    std::string gltfPath;
    std::string name;
    uint32_t id;
};

struct TextureInfo
{
    TextureProperties textureProperties;
    std::filesystem::path willtexturePath;
    std::string texturePath;
    std::string name;
    uint32_t id;
};

}

#endif //SERIALIZER_TYPES_H
