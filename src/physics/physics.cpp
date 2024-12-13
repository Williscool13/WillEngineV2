//
// Created by William on 2024-12-06.
//

#include "physics.h"
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <thread>

#include "physics_body.h"
#include "physics_filters.h"
#include "physics_utils.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"
#include "Jolt/Physics/Collision/Shape/CylinderShape.h"
#include "Jolt/Physics/Collision/Shape/SphereShape.h"
#include "src/core/game_object.h"

Physics::Physics()
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

    // A body activation listener gets notified when bodies activate and go to sleep
    // Note that this is called from a job so whatever you do here needs to be thread safe.
    // Registering one is entirely optional.
    //MyBodyActivationListener body_activation_listener;
    //physics_system.SetBodyActivationListener(&body_activation_listener);

    // A contact listener gets notified when bodies (are about to) collide, and when they separate again.
    // Note that this is called from a job so whatever you do here needs to be thread safe.
    // Registering one is entirely optional.
    //MyContactListener contact_listener;
    //physics_system.SetContactListener(&contact_listener);

    const JPH::BoxShapeSettings groundShapeSettings(JPH::Vec3(50.0f, 1.0f, 50.0f));
    groundShapeSettings.SetEmbedded();
    const JPH::ShapeSettings::ShapeResult groundShapeResult = groundShapeSettings.Create();
    const JPH::ShapeRefC groundShape = groundShapeResult.Get();

    const JPH::BodyCreationSettings groundSettings(
        groundShape,
        JPH::Vec3(0.0f, WORLD_FLOOR_HEIGHT, 0.0f),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Static,
        Layers::NON_MOVING
    );

    auto& bodyInterface = physicsSystem->GetBodyInterface();
    bodyInterface.CreateAndAddBody(groundSettings, JPH::EActivation::DontActivate);

    // Some basic shapes
    unitCubeShape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
    unitSphereShape = new JPH::SphereShape(0.5f);
    unitCapsuleShape = new JPH::CapsuleShape(0.5f, 0.25f);
    unitCylinderShape = new JPH::CylinderShape(0.5f, 0.25f);
}

Physics::~Physics()
{
    cleanup();
}

void Physics::cleanup()
{
    if (physicsSystem) {
        for (const auto& [id, body] : physicsBodies) {
            physicsSystem->GetBodyInterface().RemoveBody(body.bodyId);
            physicsSystem->GetBodyInterface().DestroyBody(body.bodyId);
        }
        physicsBodies.clear();

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

void Physics::update(const float deltaTime) const
{
    constexpr int collisionSteps = 1;
    physicsSystem->Update(deltaTime, collisionSteps, tempAllocator, jobSystem);

    updateTransforms();
}

JPH::BodyInterface& Physics::getBodyInterface() const
{
    return physicsSystem->GetBodyInterface();
}

void Physics::addRigidBody(GameObject* obj, const JPH::ShapeRefC& shape, const bool isDynamic)
{
    PhysicsBody body;
    body.gameObject = obj;

    const JPH::BodyCreationSettings settings(
        shape,
        physics_utils::ToJolt(obj->transform.getPosition()),
        physics_utils::ToJolt(obj->transform.getRotation()),
        isDynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
        isDynamic ? Layers::MOVING : Layers::NON_MOVING
    );

    body.bodyId = physicsSystem->GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
    physicsBodies[obj->getId()] = body;
}

void Physics::removeRigidBody(const GameObject* object)
{
    const auto it = physicsBodies.find(object->getId());
    if (it != physicsBodies.end()) {
        physicsSystem->GetBodyInterface().RemoveBody(it->second.bodyId);
        physicsSystem->GetBodyInterface().DestroyBody(it->second.bodyId);
        physicsBodies.erase(it);
    }
}

void Physics::removeRigidBodies(const std::vector<GameObject>& objects)
{
    std::vector<JPH::BodyID> bodyIDs;
    bodyIDs.reserve(objects.size());

    for (const auto& obj : objects) {
        const auto it = physicsBodies.find(obj.getId());
        if (it != physicsBodies.end()) {
            bodyIDs.push_back(it->second.bodyId);
            physicsBodies.erase(it);
        }
    }

    // Batch remove and destroy the bodies
    if (!bodyIDs.empty()) {
        auto& bodyInterface = physicsSystem->GetBodyInterface();
        bodyInterface.RemoveBodies(bodyIDs.data(), bodyIDs.size());
        bodyInterface.DestroyBodies(bodyIDs.data(), bodyIDs.size());
    }
}

void Physics::updateTransforms() const
{
    const JPH::BodyInterface& bodyInterface = getBodyInterface();
    for (auto& physicsBody : physicsBodies) {
        physicsBody.second.updateTransform(bodyInterface);
    }
}
