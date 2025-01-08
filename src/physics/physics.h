//
// Created by William on 2024-12-06.
//

#ifndef PHYSICS_H
#define PHYSICS_H

#include <Jolt/Jolt.h>
#include <Jolt/Core/TempAllocator.h>
// PhysicsSystem can only be included once, be careful
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/JobSystemThreadPool.h>

class PhysicsBody;
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

class Physics
{
public:
    static Physics* Get()
    {
        return physics;
    }

    static void Set(Physics* physics)
    {
        Physics::physics = physics;
    }

    /**
     * Application-wide physics context. Exists for the application's lifetime
     */
    static Physics* physics;

    Physics();

    ~Physics();

    void cleanup();

    void update(float deltaTime);

    [[nodiscard]] JPH::PhysicsSystem& getPhysicsSystem() const { return *physicsSystem; }

    JPH::BodyInterface& getBodyInterface() const;

    const std::unordered_map<uint32_t, PhysicsBody>& getGameObjectToPhysicsBodyMap() const { return physicsBodies; }


    JPH::BodyID addRigidBody(GameObject* obj, const JPH::ShapeRefC& shape, bool isDynamic = true);

    JPH::BodyID addRigidBody(GameObject* obj, const JPH::BodyCreationSettings& settings);

    void removeRigidBody(const GameObject* object);

    void removeRigidBodies(const std::vector<GameObject>& objects);

    void updateTransforms() const;

    void interpolateTransforms(float alpha) const;

    GameObject* getGameObjectFromBody(JPH::BodyID bodyId) const;

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

    // Constants
    static constexpr uint32_t MAX_BODIES = 10240;
    static constexpr uint32_t MAX_BODY_PAIRS = 10240;
    static constexpr uint32_t MAX_CONTACT_CONSTRAINTS = 10240;
    static constexpr uint32_t NUM_BODY_MUTEXES = 0; // 0 for default
    static constexpr uint32_t TEMP_ALLOCATOR_SIZE = 10 * 1024 * 1024; // 10MB

    static constexpr int32_t WORLD_FLOOR_HEIGHT = -20;

    std::unordered_map<uint32_t, PhysicsBody> physicsBodies;

public:
    JPH::ShapeRefC getUnitCubeShape() const { return unitCubeShape; }
    JPH::ShapeRefC getUnitSphereShape() const { return unitSphereShape; }
    JPH::ShapeRefC getUnitCapsuleShape() const { return unitCapsuleShape; }
    JPH::ShapeRefC getUnitCylinderShape() const { return unitCylinderShape; }

private: // Shapes
    JPH::ShapeRefC unitCubeShape = nullptr;
    JPH::ShapeRefC unitSphereShape = nullptr;
    JPH::ShapeRefC unitCapsuleShape = nullptr;
    JPH::ShapeRefC unitCylinderShape = nullptr;

private: // Physics Time
    static constexpr float FIXED_TIMESTEP = 1.0f/60.0f;
    float timeAccumulator = 0.0f;
};

#endif //PHYSICS_H
