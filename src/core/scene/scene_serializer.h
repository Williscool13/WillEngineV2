//
// Created by William on 2025-02-06.
//

#ifndef SCENE_SERIALIZER_H
#define SCENE_SERIALIZER_H

#include <json/json.hpp>


#include "src/core/transform.h"
#include "src/physics/physics.h"

#include "src/renderer/render_object/render_object.h"
#include "src/core/game_object/game_object.h"

namespace will_engine
{
using ordered_json = nlohmann::ordered_json;

constexpr int32_t SCENE_FORMAT_VERSION = 1;

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
    std::string gltfPath;
    std::string name;
    uint32_t id;
};

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

namespace physics
{
    inline void to_json(ordered_json& j, const PhysicsProperties& p)
    {
        j = {
            {"isActive", p.isActive},
            {"motionType", p.motionType},
            {"layer", p.layer},
            {"shapeType", p.shapeType},
            {
                "shapeParams", {
                    {"x", p.shapeParams.x},
                    {"y", p.shapeParams.y},
                    {"z", p.shapeParams.z}
                }
            }
        };
    }

    inline void from_json(const ordered_json& j, PhysicsProperties& props)
    {
        props.isActive = j["isActive"].get<bool>();
        props.motionType = j["motionType"].get<uint8_t>();
        props.layer = (j["layer"].get<uint16_t>());
        props.shapeType = j["shapeType"].get<JPH::EShapeSubType>();
        props.shapeParams = glm::vec3(
            j["shapeParams"]["x"].get<float>(),
            j["shapeParams"]["y"].get<float>(),
            j["shapeParams"]["z"].get<float>()
        );
    }
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

class Serializer
{
public: // GameObjects
    static void serializeGameObject(ordered_json& j, IHierarchical* obj, physics::Physics* physics, const std::unordered_map<uint32_t, RenderObject*>& renderObjects) // NOLINT(*-no-recursion)
    {
        if (const auto gameObject = dynamic_cast<GameObject*>(obj)) {
            j["id"] = gameObject->getId();
        }

        j["name"] = obj->getName();

        if (const auto transformable = dynamic_cast<ITransformable*>(obj)) {
            j["transform"] = transformable->getLocalTransform();
        }

        if (const auto renderable = dynamic_cast<IRenderable*>(obj)) {
            const int32_t renderRefIndex = renderable->getRenderReferenceIndex();
            if (renderRefIndex != INDEX_NONE && renderObjects.contains(renderRefIndex)) {
                j["renderReference"] = renderRefIndex;
                j["renderMeshIndex"] = renderable->getMeshIndex();
                j["renderIsVisible"] = renderable->isVisible();
                j["renderIsShadowCaster"] = renderable->isShadowCaster();
            }
        }

        if (const auto physicsBody = dynamic_cast<IPhysicsBody*>(obj)) {
            physics::PhysicsProperties properties = physics->serializeProperties(physicsBody);
            if (properties.isActive) {
                j["physics"] = properties;
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
                serializeGameObject(childJson, child, physics, renderObjects);
                children.push_back(childJson);
            }
            j["children"] = children;
        }
    }

    static bool serializeScene(IHierarchical* sceneRoot, const std::unordered_map<uint32_t, RenderObject*>& renderObjects, const std::string& filepath)
    {
        if (sceneRoot == nullptr) {
            fmt::print("Warning: Scene root is null\n");
            return false;
        }

        ordered_json rootJ;

        rootJ["version"] = EngineVersion::current();
        rootJ["metadata"] = SceneMetadata::create("Test Scene");

        ordered_json gameObjectJ;
        ordered_json renderObjectJ;

        const auto physics = physics::Physics::Get();
        serializeGameObject(gameObjectJ, sceneRoot, physics, renderObjects);
        rootJ["gameObjects"] = gameObjectJ;

        ordered_json renderObjectsJ = ordered_json::object();
        for (const auto key : renderObjects | std::views::keys) {
            const uint32_t id = key;
            renderObjectsJ[std::to_string(id)] = ordered_json::object();
        }
        rootJ["renderObjects"] = renderObjectsJ;

        std::ofstream file(filepath);
        file << rootJ.dump(4);

        return true;
    }

    static GameObject* deserializeGameObject(const ordered_json& j, IHierarchical* parent, std::unordered_map<uint32_t, RenderObject*> renderObjectMap)
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

        if (j.contains("renderReference") && j.contains("renderMeshIndex")) {
            if (dynamic_cast<IRenderable*>(gameObject)) {
                const uint32_t renderRefIndex = j["renderReference"].get<uint32_t>();
                const int32_t meshIndex = j["renderMeshIndex"].get<int32_t>();

                if (renderObjectMap.contains(renderRefIndex)) {
                    renderObjectMap[renderRefIndex]->generateMesh(gameObject, meshIndex);
                    gameObject->setVisibility(j["renderIsVisible"]);
                    gameObject->setIsShadowCaster(j["renderIsShadowCaster"]);
                } else {
                    fmt::print("Warning: Gameobject failed to find render reference\n");
                }
            }
        }

        if (j.contains("physics")) {
            if (const auto physicsBody = dynamic_cast<IPhysicsBody*>(gameObject)) {
                const physics::PhysicsProperties properties = j["physics"].get<physics::PhysicsProperties>();
                if (!physics::Physics::Get()->deserializeProperties(physicsBody, properties)) {
                    fmt::print("Warning: Gameobject failed to deserialize physics\n");
                }
            }
        }

        if (j.contains("children")) {
            const auto& children = j["children"];
            for (const auto& childJson : children) {
                IHierarchical* child = deserializeGameObject(childJson, gameObject, renderObjectMap);
                if (child != nullptr) {
                    gameObject->addChild(child);
                }
            }
        }

        if (parent != nullptr) {
            parent->addChild(gameObject);
        }

        return gameObject;
    }

    static bool deserializeScene(IHierarchical* root, ResourceManager& resourceManager, std::unordered_map<uint32_t, RenderObject*>& renderObjectMap,
                                 std::unordered_map<uint32_t, RenderObjectInfo> renderObjectInfoMap, const std::string& filepath)
    {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            fmt::print("Failed to open scene file: {}\n", filepath);
            return false;
        }

        ordered_json rootJ;
        try {
            file >> rootJ;
        } catch (const std::exception& e) {
            fmt::print("Failed to parse scene file: {}\n", e.what());
            return false;
        }

        if (!rootJ.contains("version")) {
            fmt::print("Scene file missing version information\n");
            return false;
        }

        const auto fileVersion = rootJ["version"].get<EngineVersion>();
        if (fileVersion > EngineVersion::current()) {
            fmt::print("Scene file version {} is newer than current engine version {}\n", fileVersion.toString(), EngineVersion::current().toString());
            return false;
        }

        if (rootJ.contains("renderObjects")) {
            const auto& renderObjectsJ = rootJ["renderObjects"];
            for (const auto& [id, entry] : renderObjectsJ.items()) {
                unsigned long long value = std::stoull(id);
                if (value > std::numeric_limits<uint32_t>::max()) {
                    fmt::print("ID {} is too large for uint32_t\n", id);
                    continue;
                }
                uint32_t index = static_cast<uint32_t>(value);

                if (renderObjectMap.contains(index)) {
                    continue;
                }

                if (!renderObjectInfoMap.contains(index)) {
                    fmt::print("WARNING: Attempted to deserialize render object but failed to find .willmodel file\n");
                    continue;
                }

                RenderObjectInfo& renderObjectData = renderObjectInfoMap[index];
                auto renderObject = new RenderObject(renderObjectData.gltfPath, resourceManager, index);
                renderObjectMap[index] = renderObject;
            }
        }

        if (rootJ.contains("gameObjects")) {
            for (auto child : rootJ["gameObjects"]["children"]) {
                IHierarchical* childObject = deserializeGameObject(child, root, renderObjectMap);
                root->addChild(childObject);
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
        info.id = computePathHash(gltfPath);

        nlohmann::json j;
        j["version"] = 1;
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

            // todo: .willmodel version conversion code
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
            info.gltfPath = renderObject["gltfPath"].get<std::string>();
            info.name = renderObject["name"].get<std::string>();
            info.id = renderObject["id"].get<uint32_t>();

            uint32_t computedId = computePathHash(info.gltfPath);
            if (computedId != info.id) {
                fmt::print("Warning: ID mismatch in {}, expected: {}, found: {} (not necessarily a problem).\n",
                           willmodelPath.string(), computedId, info.id);
            }

            return info;
        } catch (const std::exception& e) {
            fmt::print("Error loading willmodel file {}: {}\n", willmodelPath.string(), e.what());
            return std::nullopt;
        }
    }

    static uint32_t computePathHash(const std::filesystem::path& path)
    {
        const std::string normalizedPath = path.lexically_normal().string();
        return std::hash<std::string>{}(normalizedPath);
    }
};
} // will_engine

#endif //SCENE_SERIALIZER_H
