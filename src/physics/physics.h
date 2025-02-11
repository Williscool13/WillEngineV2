//
// Created by William on 2024-12-06.
//

#ifndef PHYSICS_H
#define PHYSICS_H

#include <Jolt/Jolt.h>
#include <Jolt/Core/HashCombine.h>
#include <Jolt/Core/StaticArray.h>
#include <Jolt/Core/Atomics.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Core/TempAllocator.h>
// PhysicsSystem can only be included once, be careful
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/JobSystemThreadPool.h>

#include "physics_types.h"
#include "physics_body.h"

class GameObject;

namespace JPH
{
class CylinderShape;
class CapsuleShape;
class SphereShape;
class BoxShape;
class ContactListener;
class BodyActivationListener;
class BroadPhaseLayerInterface;
class ObjectVsBroadPhaseLayerFilter;
class ObjectLayerPairFilter;
}

namespace will_engine::physics
{
class Physics
{
public:
    static Physics* Get() { return physics; }
    static void Set(Physics* _physics) { physics = _physics; }

    /**
     * Application-wide physics context. Exists for the application's lifetime
     */
    static Physics* physics;

    Physics();

    ~Physics();

    void cleanup();

    void update(float deltaTime);

    [[nodiscard]] JPH::PhysicsSystem& getPhysicsSystem() const { return *physicsSystem; }

    [[nodiscard]] JPH::BodyInterface& getBodyInterface() const;

    void removeRigidBody(const IPhysicsBody* pb);

    void removeRigidBodies(const std::vector<IPhysicsBody*>& objects);

    void updateTransforms() const;

    bool doesPhysicsBodyExists(const JPH::BodyID bodyId) const { return physicsObjects.contains(bodyId); }

    PhysicsObject* getPhysicsObject(JPH::BodyID bodyId);

public:
    JPH::BodyID setupRigidBody(IPhysicsBody* physicsBody, Collider* collider, JPH::EMotionType motion, JPH::ObjectLayer layer);

    PhysicsProperties serializeProperties(const IPhysicsBody* physicsBody) const;

    void setPositionAndRotation(JPH::BodyID bodyId, glm::vec3 position, glm::quat rotation, bool activate = true) const;

    void setPosition(JPH::BodyID bodyId, glm::vec3 position, bool activate = true) const;

    void setRotation(JPH::BodyID bodyId, glm::quat rotation, bool activate = true) const;

    void setPositionAndRotation(JPH::BodyID bodyId, JPH::Vec3 position, JPH::Quat rotation, bool activate = true) const;

    void setPosition(JPH::BodyID bodyId, JPH::Vec3 position, bool activate = true) const;

    void setRotation(JPH::BodyID bodyId, JPH::Quat rotation, bool activate = true) const;

    JPH::EMotionType getMotionType(const IPhysicsBody* body) const;

    JPH::ObjectLayer getLayer(const IPhysicsBody* body) const;

    void setMotionType(const IPhysicsBody* body, JPH::EMotionType motionType, JPH::EActivation activation) const;


private:
    static bool isIdentity(const BoxCollider* collider);

    static bool isIdentity(const SphereCollider* collider);

private:
    // Core systems
    JPH::PhysicsSystem* physicsSystem = nullptr;
    JPH::TempAllocatorImpl* tempAllocator = nullptr;
    JPH::JobSystemThreadPool* jobSystem = nullptr;

    // Layer/collision interfaces
    JPH::BroadPhaseLayerInterface* broadPhaseLayerInterface = nullptr;
    JPH::ObjectVsBroadPhaseLayerFilter* objectVsBroadPhaseLayerFilter = nullptr;
    JPH::ObjectLayerPairFilter* objectLayerPairFilter = nullptr;

    // Optional listeners
    JPH::ContactListener* contactListener = nullptr;
    JPH::BodyActivationListener* bodyActivationListener = nullptr;

    std::unordered_map<JPH::BodyID, PhysicsObject> physicsObjects;

public: // Shapes
    JPH::ShapeRefC getUnitCubeShape() const { return unitCubeShape; }
    JPH::ShapeRefC getUnitSphereShape() const { return unitSphereShape; }
    JPH::ShapeRefC getUnitCapsuleShape() const { return unitCapsuleShape; }
    JPH::ShapeRefC getUnitCylinderShape() const { return unitCylinderShape; }

private:
    JPH::ShapeRefC unitCubeShape = nullptr;
    JPH::ShapeRefC unitSphereShape = nullptr;
    JPH::ShapeRefC unitCapsuleShape = nullptr;
    JPH::ShapeRefC unitCylinderShape = nullptr;
};
}

#endif //PHYSICS_H
