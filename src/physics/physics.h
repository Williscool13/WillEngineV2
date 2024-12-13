//
// Created by William on 2024-12-06.
//

#ifndef PHYSICS_H
#define PHYSICS_H

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>

namespace JPH {
    class ContactListener;
    class BodyActivationListener;
    class BroadPhaseLayerInterface;
    class ObjectVsBroadPhaseLayerFilter;
    class ObjectLayerPairFilter;
}

class Physics {
public:
    Physics();
    ~Physics();

    void init();
    void cleanup();
    void update(float deltaTime);

    [[nodiscard]] JPH::PhysicsSystem& getPhysicsSystem() const { return *physicsSystem; }
    JPH::BodyInterface& getBodyInterface();

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
    static constexpr uint32_t NUM_BODY_MUTEXES = 0;  // 0 for default
    static constexpr uint32_t TEMP_ALLOCATOR_SIZE = 10 * 1024 * 1024;  // 10MB
};




#endif //PHYSICS_H
