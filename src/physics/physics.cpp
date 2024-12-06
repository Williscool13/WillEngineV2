//
// Created by William on 2024-12-06.
//

#include "physics.h"
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <thread>

#include "physics_filters.h"

Physics::Physics() {}

Physics::~Physics()
{
    cleanup();
}

void Physics::init()
{
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    tempAllocator = new JPH::TempAllocatorImpl(TEMP_ALLOCATOR_SIZE);

    jobSystem = new JPH::JobSystemThreadPool(
        JPH::cMaxPhysicsJobs,
        JPH::cMaxPhysicsBarriers,
        std::thread::hardware_concurrency() - 1
    );

    // Create layer interfaces
    broadPhaseLayerInterface = new BPLayerInterfaceImpl();
    objectVsBroadPhaseLayerFilter = new ObjectVsBroadPhaseLayerFilterImpl();
    objectLayerPairFilter = new ObjectLayerPairFilterImpl();

    // Create physics system
    physicsSystem = new JPH::PhysicsSystem();
    physicsSystem->Init(
        MAX_BODIES,
        NUM_BODY_MUTEXES,
        MAX_BODY_PAIRS,
        MAX_CONTACT_CONSTRAINTS,
        *broadPhaseLayerInterface,
        *objectVsBroadPhaseLayerFilter,
        *objectLayerPairFilter
    );
}

void Physics::cleanup()
{
    if (physicsSystem) {
        delete physicsSystem;
        physicsSystem = nullptr;
    }

    if (tempAllocator) {
        delete tempAllocator;
        tempAllocator = nullptr;
    }

    if (jobSystem) {
        delete jobSystem;
        jobSystem = nullptr;
    }

    if (broadPhaseLayerInterface) {
        delete broadPhaseLayerInterface;
        broadPhaseLayerInterface = nullptr;
    }

    if (objectVsBroadPhaseLayerFilter) {
        delete objectVsBroadPhaseLayerFilter;
        objectVsBroadPhaseLayerFilter = nullptr;
    }

    if (objectLayerPairFilter) {
        delete objectLayerPairFilter;
        objectLayerPairFilter = nullptr;
    }

    // Cleanup Jolt
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}

void Physics::update(float deltaTime)
{
    const int collisionSteps = 1; // Increase if taking larger steps than 1/60th second
    physicsSystem->Update(deltaTime, collisionSteps, tempAllocator, jobSystem);
}

JPH::BodyInterface& Physics::getBodyInterface()
{
    return physicsSystem->GetBodyInterface();
}
