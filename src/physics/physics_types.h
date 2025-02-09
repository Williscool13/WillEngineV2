//
// Created by William on 2024-12-15.
//

#ifndef PHYSICS_TYPES_H
#define PHYSICS_TYPES_H

#include <glm/glm.hpp>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

#include "Jolt/Physics/Collision/Shape/Shape.h"
#include "Jolt/Physics/Collision/Shape/SubShapeID.h"
#include "physics_body.h"


namespace will_engine::physics
{
struct RaycastHit
{
    bool hasHit = false;
    float fraction = 0.0f;
    float distance = 0.0f;
    glm::vec3 hitPosition{0.0f};
    glm::vec3 hitNormal{0.0f};
    JPH::BodyID hitBodyID;
    JPH::SubShapeID subShapeID;
};

struct PhysicsObject
{
    IPhysicsBody* physicsBody;
    JPH::BodyID bodyId;
    JPH::ShapeRefC shape = nullptr;
};

struct PhysicsProperties
{
    bool isActive{false};
    uint8_t motionType{};
    uint16_t layer{};
    std::string shapeType{};
    glm::vec3 shapeParams{}; // shape specific params, don't think it's necessary yet
};

enum class ColliderType
{
    Box = 0,
    Sphere = 1,
    Capsule = 2,
    Cylinder = 3,
};

struct Collider
{
    ColliderType type;

    explicit Collider(ColliderType type) : type(type) {}

    virtual ~Collider() = default;
};

struct BoxCollider : Collider
{
    glm::vec3 halfExtents{};

    explicit BoxCollider(const glm::vec3 halfExtents) : Collider(ColliderType::Box), halfExtents(halfExtents) {}

    ~BoxCollider() override = default;
};

struct SphereCollider : Collider
{
    float radius{};

    explicit SphereCollider() : Collider(ColliderType::Sphere), radius(1.0f) {}
    explicit SphereCollider(const int32_t radius) : Collider(ColliderType::Sphere), radius(radius) {}
    explicit SphereCollider(const float radius) : Collider(ColliderType::Sphere), radius(radius) {}

    ~SphereCollider() override = default;
};
}


#endif //PHYSICS_TYPES_H
