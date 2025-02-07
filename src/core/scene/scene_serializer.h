//
// Created by William on 2025-02-06.
//

#ifndef SCENE_SERIALIZER_H
#define SCENE_SERIALIZER_H

#include <nlohmann/json.hpp>


#include "src/core/transform.h"
#include "src/physics/physics.h"

#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"
#include "Jolt/Physics/Collision/Shape/CylinderShape.h"
#include "Jolt/Physics/Collision/Shape/SphereShape.h"

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

    static EngineVersion Current()
    {
        return {ENGINE_VERSION_MAJOR, ENGINE_VERSION_MINOR, ENGINE_VERSION_PATCH};
    }
};

struct SceneMetadata
{
    std::string name;
    std::string created;
    uint32_t formatVersion;

    static SceneMetadata Create(const std::string& sceneName)
    {
        const auto now = std::chrono::system_clock::now();
        const auto timeT = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S");

        return {
            sceneName,
            ss.str(),
            SCENE_FORMAT_VERSION
        };
    }
};

struct PhysicsProperties
{
    bool isActive{false};
    JPH::EMotionType motionType{};
    JPH::ObjectLayer layer{};
    std::string shapeType{};
    glm::vec3 shapeParams{}; // shape specific params, don't think it's necessary yet
};

struct RenderObjectEntry
{
    std::string filepath;
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

inline void from_json(ordered_json& j, Transform& t)
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

inline void to_json(ordered_json& j, const PhysicsProperties& p)
{
    j = {
        {"motionType", static_cast<int>(p.motionType)},
        {"layer", static_cast<int>(p.layer)},
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
    props.motionType = static_cast<JPH::EMotionType>(j["motionType"].get<int>());
    props.layer = static_cast<JPH::ObjectLayer>(j["layer"].get<int>());
    props.shapeType = j["shapeType"].get<std::string>();
    props.shapeParams = glm::vec3(
        j["shapeParams"]["x"].get<float>(),
        j["shapeParams"]["y"].get<float>(),
        j["shapeParams"]["z"].get<float>()
    );
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

inline void to_json(ordered_json& j, const RenderObjectEntry& entry)
{
    j = ordered_json{
        {"filepath", entry.filepath}
    };
}

inline void from_json(const ordered_json& j, RenderObjectEntry& entry)
{
    entry.filepath = j["filepath"].get<std::string>();
}

class SceneSerializer
{
public:
    static void SerializeGameObject(ordered_json& j, IHierarchical* obj, physics::Physics* physics, const std::vector<uint32_t>& renderObjectIds) // NOLINT(*-no-recursion)
    {
        if (auto gameObject = dynamic_cast<GameObject*>(obj)) {
            j["id"] = gameObject->getId();
        }

        j["name"] = obj->getName();

        if (auto transformable = dynamic_cast<ITransformable*>(obj)) {
            j["transform"] = transformable->getLocalTransform();
        }

        if (auto renderable = dynamic_cast<IRenderable*>(obj)) {
            const int32_t renderRefIndex = renderable->getRenderReferenceIndex();
            const bool hasValidReference = renderRefIndex != INDEX_NONE && std::ranges::find_if(renderObjectIds, [renderRefIndex](const uint32_t id) {
                return id == renderRefIndex;
            }) != renderObjectIds.end();

            if (hasValidReference) {
                j["renderReference"] = renderRefIndex;
                j["renderMeshIndex"] = renderable->getMeshIndex();
            }
        }

        if (auto physicsBody = dynamic_cast<IPhysicsBody*>(obj)) {
            if (!physicsBody->getPhysicsBodyId().IsInvalid()) {
                if (const physics::PhysicsObject* physicsObject = physics->getPhysicsObject(physicsBody->getPhysicsBodyId())) {
                    const JPH::BodyInterface& bodyInterface = physics->getBodyInterface();

                    PhysicsProperties props;
                    props.isActive = true;
                    props.motionType = bodyInterface.GetMotionType(physicsBody->getPhysicsBodyId());
                    props.layer = bodyInterface.GetObjectLayer(physicsBody->getPhysicsBodyId());

                    const JPH::Shape* shape = physicsObject->shape.GetPtr();
                    switch (shape->GetSubType()) {
                        case JPH::EShapeSubType::Box:
                        {
                            const auto box = static_cast<const JPH::BoxShape*>(shape);
                            props.shapeType = "box";
                            const JPH::Vec3 halfExtent = box->GetHalfExtent();
                            props.shapeParams = glm::vec3(halfExtent.GetX(), halfExtent.GetY(), halfExtent.GetZ());
                            break;
                        }
                        case JPH::EShapeSubType::Sphere:
                        {
                            const auto sphere = static_cast<const JPH::SphereShape*>(shape);
                            props.shapeType = "sphere";
                            props.shapeParams = glm::vec3(sphere->GetRadius(), 0.0f, 0.0f);
                            break;
                        }
                        case JPH::EShapeSubType::Capsule:
                        {
                            const auto capsule = static_cast<const JPH::CapsuleShape*>(shape);
                            props.shapeType = "capsule";
                            props.shapeParams = glm::vec3(capsule->GetRadius(), capsule->GetHalfHeightOfCylinder(), 0.0f);
                            break;
                        }
                        case JPH::EShapeSubType::Cylinder:
                        {
                            const auto cylinder = static_cast<const JPH::CylinderShape*>(shape);
                            props.shapeType = "cylinder";
                            props.shapeParams = glm::vec3(cylinder->GetRadius(), cylinder->GetHalfHeight(), 0.0f);
                            break;
                        }
                        default:
                            fmt::print("Warning: Unsupported physics shape subtype: {}\n", static_cast<int>(shape->GetSubType()));
                            break;
                    }

                    j["physics"] = props;
                }
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
                SerializeGameObject(childJson, child, physics, renderObjectIds);
                children.push_back(childJson);
            }
            j["children"] = children;
        }
    }

    static void SerializeScene(IHierarchical* sceneRoot, physics::Physics* physics, const std::vector<RenderObject*>& renderObjects, const std::string& filepath)
    {
        if (sceneRoot == nullptr) {
            fmt::print("Warning: Scene root is null\n");
            return;
        }

        ordered_json rootJ;

        rootJ["version"] = EngineVersion::Current();
        rootJ["metadata"] = SceneMetadata::Create("Test Scene");

        ordered_json gameObjectJ;
        ordered_json renderObjectJ;
        std::vector<uint32_t> renderObjectIndices;
        renderObjectIndices.reserve(renderObjects.size());
        std::ranges::transform(renderObjects, std::back_inserter(renderObjectIndices),
                               [](const RenderObject* obj) { return obj->getRenderReferenceIndex(); });

        SerializeGameObject(gameObjectJ, sceneRoot, physics, renderObjectIndices);
        rootJ["gameObjects"] = gameObjectJ;

        ordered_json renderObjectsJ = ordered_json::object();
        for (const RenderObject* renderObject : renderObjects) {
            const uint32_t id = renderObject->getRenderReferenceIndex();
            renderObjectsJ[std::to_string(id)] = RenderObjectEntry{
                .filepath = renderObject->getFilePath().string(),
            };
        }
        rootJ["renderObjects"] = renderObjectsJ;

        std::ofstream file(filepath);
        file << rootJ.dump(4);
    }

    static GameObject* DeserializeGameObject(const ordered_json& j, physics::Physics* physics, ResourceManager& resourceManager)
    {
        // auto* obj = new GameObject(j["name"].get<std::string>());
        // obj->transform = j["transform"].get<Transform>();
        //
        // // Create render object if needed
        // if (j.contains("renderObject")) {
        //     std::string path = j["renderObject"].get<std::string>();
        //     auto* renderObj = new RenderObject(context, resourceManager, path, layouts);
        //     renderObj->generateGameObject(obj);
        // }
        //
        // // Deserialize physics properties if present
        // if (j.contains("physics")) {
        //     PhysicsProperties props = j["physics"].get<PhysicsProperties>();
        //
        //     // Create physics shape based on type
        //     JPH::ShapeRefC shape;
        //     if (props.shapeType == "box") {
        //         shape = new JPH::BoxShape(JPH::Vec3(
        //             props.shapeParams.x,
        //             props.shapeParams.y,
        //             props.shapeParams.z
        //         ));
        //     }
        //     // Add other shape types as needed
        //
        //     if (shape != nullptr) {
        //         JPH::BodyCreationSettings settings(
        //             shape,
        //             physics_utils::ToJolt(obj->transform.getPosition()),
        //             physics_utils::ToJolt(obj->transform.getRotation()),
        //             props.motionType,
        //             props.layer
        //         );
        //         physics->addRigidBody(obj, settings);
        //     }
        // }
        //
        // // Deserialize children recursively
        // if (j.contains("children")) {
        //     for (const auto& childJson : j["children"]) {
        //         GameObject* child = DeserializeGameObject(childJson, physics, context, resourceManager, layouts);
        //         obj->addChild(child);
        //     }
        // }
        //
        // return obj;
        return nullptr;
    }

    static GameObject* DeserializeScene(physics::Physics* physics, ResourceManager& resourceManager, const std::string& filepath)
    {
        // Read JSON from file
        std::ifstream file(filepath);
        ordered_json j;
        file >> j;

        return DeserializeGameObject(j, physics, resourceManager);
    }

private:
    static std::unordered_map<uint32_t, RenderObjectEntry> ParseRenderObjects(const ordered_json& j)
    {
        std::unordered_map<uint32_t, RenderObjectEntry> result;

        const auto& renderObjects = j["renderObjects"];
        for (const auto& [idStr, entry] : renderObjects.items()) {
            uint32_t id = std::stoul(idStr);
            result[id] = entry.get<RenderObjectEntry>();
        }

        return result;
    }
};
} // will_engine

#endif //SCENE_SERIALIZER_H
