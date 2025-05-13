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
    JPH::EShapeSubType shapeType;
    glm::vec3 shapeParams{}; // shape specific params, don't think it's necessary yet
};
}


#endif //PHYSICS_TYPES_H
