//
// Created by William on 2025-02-22.
//

#include "rigid_body_component.h"

#include <fmt/format.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include "imgui.h"
#include "src/core/game_object/transformable.h"
#include "src/physics/physics.h"
#include "src/physics/physics_constants.h"
#include "src/physics/physics_filters.h"
#include "src/physics/physics_utils.h"


namespace will_engine::physics
{
using ordered_json = nlohmann::ordered_json;

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

namespace will_engine::components
{
RigidBodyComponent::RigidBodyComponent(const std::string& name)
    : Component(name)
{}

RigidBodyComponent::~RigidBodyComponent() = default;

void RigidBodyComponent::beginPlay()
{
    Component::beginPlay();

    if (!physics::Physics::Get()->deserializeProperties(this, deserializedPhysicsProperties)) {
        fmt::print("Warning: Gameobject failed to deserialize physics\n");
    }
}

void RigidBodyComponent::update(const float deltaTime)
{
    Component::update(deltaTime);
}

void RigidBodyComponent::beginDestroy()
{
    Component::beginDestroy();

    if (bodyId.GetIndex() != JPH::BodyID::cMaxBodyIndex) {
        if (physics::Physics* physics = physics::Physics::Get()) {
            physics->removeRigidBody(this);
            bodyId = JPH::BodyID(JPH::BodyID::cMaxBodyIndex);
        }
    }
}

void RigidBodyComponent::onEnable()
{
    Component::onEnable();
    // Add enable logic here
}

void RigidBodyComponent::onDisable()
{
    Component::onDisable();
    // Add disable logic here
}

void RigidBodyComponent::setOwner(IComponentContainer* owner)
{
    Component::setOwner(owner);

    parent = dynamic_cast<ITransformable*>(owner);
    if (!parent) {
        fmt::print("Attempted to attach a rigidbody to an IComponentContainer that does not implement ITransformable. This component will not have an effect.\n");
    }
}

void RigidBodyComponent::setGameTransformFromPhysics(const glm::vec3& position, const glm::quat& rotation)
{
    parent->setGlobalTransformFromPhysics(position, rotation);
}

void RigidBodyComponent::setPhysicsTransformFromGame(const glm::vec3& position, const glm::quat& rotation)
{
    if (bodyId.GetIndex() == JPH::BodyID::cMaxBodyIndex) { return; }
    physics::Physics::Get()->setPositionAndRotation(bodyId, position, rotation);
}

glm::vec3 RigidBodyComponent::getGlobalPosition()
{
    return parent->getGlobalPosition();
}

glm::quat RigidBodyComponent::getGlobalRotation()
{
    return parent->getGlobalRotation();
}

void RigidBodyComponent::serialize(ordered_json& j)
{
    Component::serialize(j);

    physics::PhysicsProperties properties = physics::Physics::Get()->serializeProperties(this);
    if (properties.isActive) {
        j["properties"] = properties;
    }
}

void RigidBodyComponent::deserialize(ordered_json& j)
{
    Component::deserialize(j);

    if (j.contains("properties")) {
        deserializedPhysicsProperties = j["properties"].get<physics::PhysicsProperties>();
    }
}

void RigidBodyComponent::updateRenderImgui()
{
    // IPhysicsBody
    if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
        const JPH::BodyID bodyId = getPhysicsBodyId();

        if (bodyId.GetIndex() == JPH::BodyID::cMaxBodyIndex) {
            static int selectedShape = 0;
            const char* shapes[] = {"Box", "Sphere", "Capsule", "Cylinder"};
            ImGui::Combo("Shape", &selectedShape, shapes, 4);
            static bool useUnitShape = true;
            glm::vec3 shapeParams = glm::vec3(1.0f);

            // Shape Generation
            {
                ImGui::Checkbox("Use Unit Shape", &useUnitShape);
                if (!useUnitShape) {
                    switch (selectedShape) {
                        case 0: // Box
                        {
                            static glm::vec3 boxHalfExtents(1.0f);
                            ImGui::Text("Half Extents");
                            ImGui::DragFloat3("##BoxHalfExtents", &boxHalfExtents.x, 0.1f, 0.1f, 100.0f);
                            shapeParams = boxHalfExtents;
                            break;
                        }
                        case 1: // Sphere
                        {
                            static float radius = 1.0f;
                            ImGui::Text("Radius");
                            ImGui::DragFloat("##SphereRadius", &radius, 0.1f, 0.1f, 100.0f);
                            shapeParams = glm::vec3(radius, 0.0f, 0.0f);
                            break;
                        }
                        case 2: // Capsule
                        {
                            static float radius = 1.0f;
                            static float halfHeight = 1.0f;
                            ImGui::Text("Radius");
                            ImGui::DragFloat("##CapsuleRadius", &radius, 0.1f, 0.1f, 100.0f);
                            ImGui::Text("Half Height");
                            ImGui::DragFloat("##CapsuleHeight", &halfHeight, 0.1f, 0.1f, 100.0f);
                            shapeParams = glm::vec3(radius, halfHeight, 0.0f);
                            break;
                        }
                        case 3: // Cylinder
                        {
                            static float radius = 1.0f;
                            static float halfHeight = 1.0f;
                            ImGui::Text("Radius");
                            ImGui::DragFloat("##CylinderRadius", &radius, 0.1f, 0.1f, 100.0f);
                            ImGui::Text("Half Height");
                            ImGui::DragFloat("##CylinderHeight", &halfHeight, 0.1f, 0.1f, 100.0f);
                            shapeParams = glm::vec3(radius, halfHeight, 0.0f);
                            break;
                        }
                        default:
                            shapeParams = glm::vec3(1.0f);
                            break;
                    }
                }
                else {
                    switch (selectedShape) {
                        case 0:
                            shapeParams = physics::UNIT_CUBE;
                            break;
                        case 1:
                            shapeParams = physics::UNIT_SPHERE;
                            break;
                        case 2:
                            shapeParams = physics::UNIT_CAPSULE;
                            break;
                        case 3:
                            shapeParams = physics::UNIT_CYLINDER;
                            break;
                        default:
                            shapeParams = glm::vec3(1.0f);
                            break;
                    }
                }
            }

            static auto motionType = JPH::EMotionType::Dynamic;
            const char* motionTypes[] = {"Static", "Kinematic", "Dynamic"};
            int currentType = static_cast<int>(motionType);
            ImGui::Combo("Motion Type", &currentType, motionTypes, 3);
            motionType = static_cast<JPH::EMotionType>(currentType);

            const char* layers[] = {"Non-Moving", "Moving", "Player", "Terrain"};
            static JPH::ObjectLayer layer = physics::Layers::MOVING;
            int currentLayer = layer;
            ImGui::Combo("Layer", &currentLayer, layers, 4);
            layer = static_cast<JPH::ObjectLayer>(currentLayer);


            if (ImGui::Button("Add Rigidbody")) {
                JPH::EShapeSubType shapeType;

                switch (selectedShape) {
                    case 0:
                        shapeType = JPH::EShapeSubType::Box;
                        break;
                    case 1:
                        shapeType = JPH::EShapeSubType::Sphere;
                        break;
                    case 2:
                        shapeType = JPH::EShapeSubType::Capsule;
                        break;
                    case 3:
                        shapeType = JPH::EShapeSubType::Cylinder;
                        break;
                    default: shapeType = JPH::EShapeSubType::Box;
                        break;
                }

                physics::Physics::Get()->setupRigidbody(
                    this,
                    shapeType,
                    shapeParams,
                    motionType,
                    layer
                );
            }
        }
        else {
            ImGui::Text("Body Id: %u", bodyId);

            ImGui::Separator();

            // Layer
            {
                JPH::ObjectLayer layer = physics::PhysicsUtils::getObjectLayer(bodyId);
                if (layer < physics::Layers::NUM_LAYERS) {
                    auto layerName = physics::Layers::layerNames[layer];
                    ImGui::Text("Layer: %s", layerName);
                }
                else {
                    ImGui::Text("Layer: Invalid Layer Found greater than NUM_LAYERS");
                }
            }

            ImGui::Separator();

            // Shape
            {
                if (physics::PhysicsObject* physicsObject = physics::Physics::Get()->getPhysicsObject(bodyId)) {
                    const JPH::ShapeRefC shape = physicsObject->shape;
                    ImGui::Text("Shape: ");
                    ImGui::SameLine();

                    switch (physicsObject->shape->GetSubType()) {
                        case JPH::EShapeSubType::Box:
                        {
                            const auto box = static_cast<const JPH::BoxShape*>(shape.GetPtr());
                            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Box");
                            const JPH::Vec3 halfExtent = box->GetHalfExtent();
                            ImGui::Columns(2);
                            ImGui::Text("Half Extents");
                            ImGui::NextColumn();
                            ImGui::Text("X: %.2f  Y: %.2f  Z: %.2f", halfExtent.GetX(), halfExtent.GetY(), halfExtent.GetZ());
                            ImGui::Columns(1);
                            break;
                        }
                        case JPH::EShapeSubType::Sphere:
                        {
                            const auto sphere = static_cast<const JPH::SphereShape*>(shape.GetPtr());
                            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Sphere");
                            ImGui::Columns(2);
                            ImGui::Text("Radius");
                            ImGui::NextColumn();
                            ImGui::Text("%.2f", sphere->GetRadius());
                            ImGui::Columns(1);
                            break;
                        }
                        case JPH::EShapeSubType::Capsule:
                        {
                            const auto capsule = static_cast<const JPH::CapsuleShape*>(shape.GetPtr());
                            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Capsule");
                            ImGui::Columns(2);
                            ImGui::Text("Radius");
                            ImGui::NextColumn();
                            ImGui::Text("%.2f", capsule->GetRadius());
                            ImGui::NextColumn();
                            ImGui::Text("Half Height");
                            ImGui::NextColumn();
                            ImGui::Text("%.2f", capsule->GetHalfHeightOfCylinder());
                            ImGui::Columns(1);
                            break;
                        }
                        case JPH::EShapeSubType::Cylinder:
                        {
                            const auto cylinder = static_cast<const JPH::CylinderShape*>(shape.GetPtr());
                            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Cylinder");
                            ImGui::Columns(2);
                            ImGui::Text("Radius");
                            ImGui::NextColumn();
                            ImGui::Text("%.2f", cylinder->GetRadius());
                            ImGui::NextColumn();
                            ImGui::Text("Half Height");
                            ImGui::NextColumn();
                            ImGui::Text("%.2f", cylinder->GetHalfHeight());
                            ImGui::Columns(1);
                            break;
                        }
                        default:
                            ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.4f, 1.0f), "Unknown");
                            break;
                    }
                }
            }

            ImGui::Separator();

            // MotionType
            {
                JPH::EMotionType currentMotionType = physics::Physics::Get()->getMotionType(this);
                if (currentMotionType == JPH::EMotionType::Static) {
                    ImGui::Text("Motion Type: Static (Unable to change)");
                }
                else {
                    const char* motionTypes[] = {"Kinematic", "Dynamic"};
                    int currentType = static_cast<int>(currentMotionType) - 1;

                    if (ImGui::Combo("Motion Type", &currentType, motionTypes, 2)) {
                        const JPH::EMotionType newMotionType = static_cast<JPH::EMotionType>(currentType + 1);
                        switch (newMotionType) {
                            case JPH::EMotionType::Kinematic:
                                physics::Physics::Get()->setMotionType(
                                    this,
                                    static_cast<JPH::EMotionType>(currentType + 1),
                                    JPH::EActivation::DontActivate
                                );
                                break;
                            case JPH::EMotionType::Dynamic:
                                physics::Physics::Get()->setMotionType(
                                    this,
                                    static_cast<JPH::EMotionType>(currentType + 1),
                                    JPH::EActivation::Activate
                                );
                                break;
                            default:
                                break;
                        }
                        physics::PhysicsUtils::resetVelocity(bodyId);
                    }

                    glm::vec3 velocity = physics::PhysicsUtils::getLinearVelocity(bodyId);
                    ImGui::Text("Velocity: %.2f, %.2f, %.2f", velocity.x, velocity.y, velocity.z);

                    if (ImGui::Button("Reset Velocity")) {
                        physics::PhysicsUtils::resetVelocity(bodyId);
                    }


                    if (ImGui::Button("Remove Rigidbody")) {
                        physics::Physics::Get()->releaseRigidbody(this);
                    }
                }
            }
        }
    }
}
} // namespace will_engine::components
