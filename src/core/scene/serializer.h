//
// Created by William on 2025-02-06.
//

#ifndef SCENE_SERIALIZER_H
#define SCENE_SERIALIZER_H

#include <json/json.hpp>


#include "map.h"
#include "serializer_types.h"
#include "src/core/transform.h"
#include "src/core/game_object/game_object.h"
#include "src/core/game_object/components/component.h"
#include "src/core/game_object/components/component_factory.h"
#include "src/renderer/assets/render_object/render_object.h"
#include "src/renderer/assets/texture/texture.h"


namespace will_engine
{
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

namespace terrain
{
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

class Serializer
{
public: // GameObjects
    static void serializeGameObject(ordered_json& j, IHierarchical* obj)
    {
        if (const auto gameObject = dynamic_cast<GameObject*>(obj)) {
            j["id"] = gameObject->getId();
        }

        j["name"] = obj->getName();

        if (const auto transformable = dynamic_cast<ITransformable*>(obj)) {
            j["transform"] = transformable->getLocalTransform();
        }

        if (const auto componentContainer = dynamic_cast<IComponentContainer*>(obj)) {
            const std::vector<components::Component*> components = componentContainer->getAllComponents();
            if (components.size() > 0) {
                ordered_json componentsJson;
                for (const auto& component : components) {
                    ordered_json componentData;
                    component->serialize(componentData);
                    componentsJson[component->getComponentType()] = componentData;
                    componentsJson[component->getComponentType()]["componentName"] = component->getComponentName();
                }
                j["components"] = componentsJson;
            }
        }

        if (!obj->getChildren().empty()) {
            ordered_json children = ordered_json::array();
            for (IHierarchical* child : obj->getChildren()) {
                ordered_json childJson;
                if (child == nullptr) {
                    fmt::print("SerializeGameObject: null game object found in IHierarchical chain (child of {})\n", obj->getName());
                    continue;
                }
                serializeGameObject(childJson, child);
                children.push_back(childJson);
            }
            j["children"] = children;
        }
    }

    static bool serializeMap(IHierarchical* map, ordered_json& rootJ, const std::filesystem::path& filepath)
    {
        if (map == nullptr) {
            fmt::print("Warning: map is null\n");
            return false;
        }

        rootJ["version"] = EngineVersion::current();
        rootJ["metadata"] = SceneMetadata::create(map->getName().data());

        if (const auto componentContainer = dynamic_cast<IComponentContainer*>(map)) {
            const std::vector<components::Component*> components = componentContainer->getAllComponents();
            if (components.size() > 0) {
                ordered_json componentsJson;
                for (const auto& component : components) {
                    ordered_json componentData;
                    component->serialize(componentData);
                    componentsJson[component->getComponentType()] = componentData;
                    componentsJson[component->getComponentType()]["componentName"] = component->getComponentName();
                }
                rootJ["rootComponents"] = componentsJson;
            }
        }

        ordered_json gameObjectJ;
        ordered_json renderObjectJ;

        serializeGameObject(gameObjectJ, map);
        rootJ["gameObjects"] = gameObjectJ;

        std::ofstream file(filepath);
        file << rootJ.dump(4);

        return true;
    }

    static GameObject* deserializeGameObject(const ordered_json& j, IHierarchical* parent)
    {
        const auto gameObject = new GameObject();

        if (j.contains("id")) {
            gameObject->setId(j["id"].get<uint32_t>());
        }

        if (j.contains("name")) {
            gameObject->setName(j["name"].get<std::string>());
        }

        if (j.contains("transform")) {
            if (const auto transformable = dynamic_cast<ITransformable*>(gameObject)) {
                transformable->setLocalTransform(j["transform"].get<Transform>());
            }
        }

        if (j.contains("components")) {
            if (const auto componentContainer = dynamic_cast<IComponentContainer*>(gameObject)) {
                const auto& components = j["components"];
                for (const auto& [componentType, componentData] : components.items()) {
                    std::string componentName;
                    if (componentData.contains("componentName")) {
                        componentName = componentData["componentName"].get<std::string>();
                    }
                    else {
                        componentName = componentType;
                    }

                    auto& factory = components::ComponentFactory::getInstance();
                    auto newComponent = factory.createComponent(componentType, componentName);

                    if (newComponent) {
                        auto orderedComponentData = ordered_json(componentData);
                        const auto _component = componentContainer->addComponent(std::move(newComponent));
                        _component->deserialize(orderedComponentData);
                    }
                }
            }
        }

        if (j.contains("children")) {
            const auto& children = j["children"];
            for (const auto& childJson : children) {
                if (IHierarchical* child = deserializeGameObject(childJson, gameObject)) {
                    gameObject->addChild(child);
                }
            }
        }

        if (parent != nullptr) {
            parent->addChild(gameObject);
        }

        return gameObject;
    }

    static bool deserializeMap(IHierarchical* root, ordered_json& rootJ)
    {
        if (rootJ.contains("gameObjects")) {
            for (auto child : rootJ["gameObjects"]["children"]) {
                IHierarchical* childObject = deserializeGameObject(child, root);
                root->addChild(childObject);
            }
        }

        if (rootJ.contains("rootComponents")) {
            if (const auto componentContainer = dynamic_cast<IComponentContainer*>(root)) {
                const auto& components = rootJ["rootComponents"];
                for (const auto& [componentType, componentData] : components.items()) {
                    std::string componentName;
                    if (componentData.contains("componentName")) {
                        componentName = componentData["componentName"].get<std::string>();
                    }
                    else {
                        componentName = componentType;
                    }

                    auto& factory = components::ComponentFactory::getInstance();
                    auto newComponent = factory.createComponent(componentType, componentName);

                    if (newComponent) {
                        auto orderedComponentData = ordered_json(componentData);
                        const auto _component = componentContainer->addComponent(std::move(newComponent));
                        _component->deserialize(orderedComponentData);
                    }
                }
            }
        }

        return true;
    }

public: // Render Objects
    static bool generateWillModel(const std::filesystem::path& gltfPath, const std::filesystem::path& outputPath)
    {
        RenderObjectInfo info;
        info.gltfPath = gltfPath.string();
        info.name = gltfPath.stem().string();
        info.id = computePathHash(info.gltfPath);

        nlohmann::json j;
        j["version"] = WILL_MODEL_FORMAT_VERSION;
        j["renderObject"] = {
            {"gltfPath", info.gltfPath},
            {"name", info.name},
            {"id", info.id}
        };

        try {
            std::ofstream o(outputPath);
            o << std::setw(4) << j << std::endl;
            return true;
        } catch (const std::exception& e) {
            fmt::print("Failed to write willmodel file: {}\n", e.what());
            return false;
        }
    }

    static std::optional<RenderObjectInfo> loadWillModel(const std::filesystem::path& willmodelPath)
    {
        try {
            std::ifstream i(willmodelPath);
            if (!i.is_open()) {
                fmt::print("Failed to open willmodel file: {}\n", willmodelPath.string());
                return std::nullopt;
            }

            nlohmann::json j;
            i >> j;

            if (!j.contains("version") || j["version"] != 1) {
                fmt::print("Invalid or unsupported willmodel version in: {}\n", willmodelPath.string());
                return std::nullopt;
            }

            if (!j.contains("renderObject")) {
                fmt::print("No renderObject data found in: {}\n", willmodelPath.string());
                return std::nullopt;
            }

            const auto& renderObject = j["renderObject"];

            RenderObjectInfo info;
            info.willmodelPath = willmodelPath;
            info.gltfPath = renderObject["gltfPath"].get<std::string>();
            info.name = renderObject["name"].get<std::string>();
            info.id = renderObject["id"].get<uint32_t>();

            uint32_t computedId = computePathHash(info.gltfPath);
            if (computedId != info.id) {
                fmt::print("Warning: ID mismatch in {}, expected: {}, found: {} (not necessarily a problem).\n", willmodelPath.string(), computedId, info.id);
                info.id = computedId;
            }

            return info;
        } catch (const std::exception& e) {
            fmt::print("Error loading willmodel file {}: {}\n", willmodelPath.string(), e.what());
            return std::nullopt;
        }
    }

public: // Textures
    static bool generateWillTexture(const std::filesystem::path& texturePath, const std::filesystem::path& outputPath)
    {
        TextureInfo info;
        info.texturePath = texturePath.string();
        info.name = outputPath.stem().string();
        info.id = computePathHash(texturePath.string());

        nlohmann::json j;
        j["version"] = WILL_TEXTURE_FORMAT_VERSION;
        j["texture"] = {
            {"gltfPath", info.texturePath},
            {"name", info.name},
            {"id", info.id},
            {"textureProperties", info.textureProperties},
        };

        try {
            std::ofstream o(outputPath);
            o << std::setw(4) << j << std::endl;
            return true;
        } catch (const std::exception& e) {
            fmt::print("Failed to write willtexture file: {}\n", e.what());
            return false;
        }
    }

    static std::optional<TextureInfo> loadWillTexture(const std::filesystem::path& willtexturePath)
    {
        try {
            std::ifstream i(willtexturePath);
            if (!i.is_open()) {
                fmt::print("Failed to open willtexture file: {}\n", willtexturePath.string());
                return std::nullopt;
            }

            nlohmann::json j;
            i >> j;

            if (!j.contains("version") || j["version"] != 1) {
                fmt::print("Invalid or unsupported willtexture version in: {}\n", willtexturePath.string());
                return std::nullopt;
            }

            if (!j.contains("texture")) {
                fmt::print("No texture data found in: {}\n", willtexturePath.string());
                return std::nullopt;
            }

            const auto& texture = j["texture"];

            TextureInfo info;
            info.willtexturePath = willtexturePath;
            info.texturePath = texture.value<std::string>("gltfPath", "");
            info.name = texture.value<std::string>("name", "");
            info.id = texture.value<uint32_t>("id", 0);;
            info.textureProperties = texture.value<TextureProperties>("textureProperties", {});

            uint32_t computedId = computePathHash(info.texturePath);
            if (computedId != info.id) {
                fmt::print("Warning: ID mismatch in {}, expected: {}, found: {} (not necessarily a problem).\n", willtexturePath.string(), computedId, info.id);
                info.id = computedId;
            }

            return info;
        } catch (const std::exception& e) {
            fmt::print("Error loading willtexture file {}: {}\n", willtexturePath.string(), e.what());
            return std::nullopt;
        }
    }

public: //
    static uint32_t computePathHash(const std::filesystem::path& path)
    {
        const std::string normalizedPath = path.lexically_normal().string();
        return std::hash<std::string>{}(normalizedPath);
    }
};
} // will_engine

#endif //SCENE_SERIALIZER_H
