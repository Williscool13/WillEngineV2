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

void RigidBodyComponent::releaseRigidBody()
{
    if (hasRigidBody()) {
        if (physics::Physics* physics = physics::Physics::get()) {
            physics->removeRigidBody(this);
            bodyId = JPH::BodyID(JPH::BodyID::cMaxBodyIndex);
        }
    }
}

void RigidBodyComponent::beginDestroy()
{
    Component::beginDestroy();

    releaseRigidBody();
}

void RigidBodyComponent::setOwner(IComponentContainer* owner)
{
    Component::setOwner(owner);

    transformableOwner = dynamic_cast<ITransformable*>(owner);
    if (!transformableOwner) {
        fmt::print("Attempted to attach a rigidbody to an IComponentContainer that does not implement ITransformable. This component will not have an effect.\n");
    }
}

void RigidBodyComponent::setGameTransformFromPhysics(const glm::vec3& position, const glm::quat& rotation)
{
    transformableOwner->setGlobalTransformFromPhysics(position, rotation);
}

void RigidBodyComponent::setPhysicsTransformFromGame(const glm::vec3& position, const glm::quat& rotation)
{
    if (!hasRigidBody()) { return; }
    physics::Physics::get()->setPositionAndRotation(bodyId, position, rotation);
}

glm::vec3 RigidBodyComponent::getGlobalPosition()
{
    return transformableOwner->getGlobalPosition();
}

glm::quat RigidBodyComponent::getGlobalRotation()
{
    return transformableOwner->getGlobalRotation();
}

void RigidBodyComponent::serialize(ordered_json& j)
{
    Component::serialize(j);

    physics::PhysicsProperties properties = physics::Physics::get()->serializeProperties(this);
    if (properties.isActive) {
        j["properties"] = properties;
    }
}

void RigidBodyComponent::deserialize(ordered_json& j)
{
    Component::deserialize(j);

    if (j.contains("properties")) {
        const auto properties = j["properties"].get<physics::PhysicsProperties>();
        if (!physics::Physics::get()->deserializeProperties(this, properties)) {
            fmt::print("Warning: RigidBodyComponent failed to deserialize physics\n");
        }
    }
}

void RigidBodyComponent::updateRenderImgui()
{
    if (hasRigidBody()) {
        ImGui::Text("Body Id: %u  | ", bodyId);

        ImGui::SameLine();

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

        // Shape
        {
            if (physics::PhysicsObject* physicsObject = physics::Physics::get()->getPhysicsObject(bodyId)) {
                const JPH::ShapeRefC shape = physicsObject->shape;
                ImGui::Text("Shape: ");
                ImGui::SameLine();

                switch (physicsObject->shape->GetSubType()) {
                    case JPH::EShapeSubType::Box:
                    {
                        const auto box = static_cast<const JPH::BoxShape*>(shape.GetPtr());
                        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Box");
                        const JPH::Vec3 halfExtent = box->GetHalfExtent();
                        ImGui::Text("Half Extents");
                        ImGui::SameLine();
                        ImGui::Text("X: %.2f  Y: %.2f  Z: %.2f", halfExtent.GetX(), halfExtent.GetY(), halfExtent.GetZ());
                        break;
                    }
                    case JPH::EShapeSubType::Sphere:
                    {
                        const auto sphere = static_cast<const JPH::SphereShape*>(shape.GetPtr());
                        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Sphere");
                        ImGui::Text("Radius");
                        ImGui::SameLine();
                        ImGui::Text("%.2f", sphere->GetRadius());
                        break;
                    }
                    case JPH::EShapeSubType::Capsule:
                    {
                        const auto capsule = static_cast<const JPH::CapsuleShape*>(shape.GetPtr());
                        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Capsule");
                        ImGui::Text("Radius");
                        ImGui::SameLine();
                        ImGui::Text("%.2f", capsule->GetRadius());
                        ImGui::SameLine();
                        ImGui::Text("Half Height");
                        ImGui::SameLine();
                        ImGui::Text("%.2f", capsule->GetHalfHeightOfCylinder());
                        break;
                    }
                    case JPH::EShapeSubType::Cylinder:
                    {
                        const auto cylinder = static_cast<const JPH::CylinderShape*>(shape.GetPtr());
                        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Cylinder");
                        ImGui::Text("Radius");
                        ImGui::SameLine();
                        ImGui::Text("%.2f", cylinder->GetRadius());
                        ImGui::SameLine();
                        ImGui::Text("Half Height");
                        ImGui::SameLine();
                        ImGui::Text("%.2f", cylinder->GetHalfHeight());
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
            JPH::EMotionType currentMotionType = physics::Physics::get()->getMotionType(this);
            if (currentMotionType == JPH::EMotionType::Static) {
                ImGui::Text("Motion Type: Static (Immutable)");
            }
            else {
                const char* motionTypes[] = {"Kinematic", "Dynamic"};
                int currentType = static_cast<int>(currentMotionType) - 1;

                if (ImGui::Combo("Motion Type", &currentType, motionTypes, 2)) {
                    const JPH::EMotionType newMotionType = static_cast<JPH::EMotionType>(currentType + 1);
                    switch (newMotionType) {
                        case JPH::EMotionType::Kinematic:
                            physics::Physics::get()->setMotionType(
                                this,
                                static_cast<JPH::EMotionType>(currentType + 1),
                                JPH::EActivation::DontActivate
                            );
                            break;
                        case JPH::EMotionType::Dynamic:
                            physics::Physics::get()->setMotionType(
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
                    physics::Physics::get()->releaseRigidbody(this);
                }
            }
        }
    }

    ImGui::Separator();

    static int selectedShape = 0;
    const char* shapes[] = {"Box", "Sphere", "Capsule", "Cylinder"};
    ImGui::Combo("Shape", &selectedShape, shapes, 4);
    static bool useUnitShape = true;
    glm::vec3 shapeParams;

    // Shape Generation
    ImGui::SameLine();
    ImGui::Checkbox("Use Unit Shape", &useUnitShape);
    if (useUnitShape) {
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
    else {
        switch (selectedShape) {
            case 0: // Box
            {
                static glm::vec3 boxHalfExtents(1.0f);
                ImGui::DragFloat3("Box Half Extents", &boxHalfExtents.x, 0.1f, 0.1f, 100.0f);
                shapeParams = boxHalfExtents;
                break;
            }
            case 1: // Sphere
            {
                static float radius = 1.0f;
                ImGui::DragFloat("Sphere Radius", &radius, 0.1f, 0.1f, 100.0f);
                shapeParams = glm::vec3(radius, 0.0f, 0.0f);
                break;
            }
            case 2: // Capsule
            {
                static float radius = 1.0f;
                static float halfHeight = 1.0f;

                ImGui::DragFloat("Capsule Radius", &radius, 0.1f, 0.1f, 100.0f);
                ImGui::SameLine();
                ImGui::DragFloat("Capsule Height", &halfHeight, 0.1f, 0.1f, 100.0f);
                shapeParams = glm::vec3(radius, halfHeight, 0.0f);
                break;
            }
            case 3: // Cylinder
            {
                static float radius = 1.0f;
                static float halfHeight = 1.0f;
                ImGui::DragFloat("CylinderRadius", &radius, 0.1f, 0.1f, 100.0f);
                ImGui::SameLine();
                ImGui::DragFloat("CylinderHeight", &halfHeight, 0.1f, 0.1f, 100.0f);
                shapeParams = glm::vec3(radius, halfHeight, 0.0f);
                break;
            }
            default:
                shapeParams = glm::vec3(1.0f);
                break;
        }
    }

    static auto motionType = JPH::EMotionType::Dynamic;
    const char* motionTypes[] = {"Static", "Kinematic", "Dynamic"};
    int currentType = static_cast<int>(motionType);
    ImGui::Combo("Motion Type", &currentType, motionTypes, 3);
    motionType = static_cast<JPH::EMotionType>(currentType);
    ImGui::SameLine();
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

        if (hasRigidBody()) {
            releaseRigidBody();
        }
        physics::Physics::get()->setupRigidbody(
            this,
            shapeType,
            shapeParams,
            motionType,
            layer
        );
    }
}
} // namespace will_engine::components
