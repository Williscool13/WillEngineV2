//
// Created by William on 2024-12-06.
//

#include "physics.h"

#include <thread>

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include "Jolt/Physics/Body/BodyCreationSettings.h"
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"
#include "Jolt/Physics/Collision/Shape/CylinderShape.h"
#include "Jolt/Physics/Collision/Shape/SphereShape.h"

#include "physics_constants.h"
#include "physics_filters.h"
#include "physics_utils.h"
#include "src/core/game_object/physics_body.h"


namespace will_engine::physics
{
Physics* Physics::physics = nullptr;

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
        Layers::TERRAIN
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
        for (const auto physicsObject : physicsObjects) {
            physicsSystem->GetBodyInterface().RemoveBody(physicsObject.first);
            physicsSystem->GetBodyInterface().DestroyBody(physicsObject.first);
        }

        physicsObjects.clear();

        unitCubeShape = nullptr;
        unitSphereShape = nullptr;
        unitCapsuleShape = nullptr;
        unitCylinderShape = nullptr;

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

void Physics::update(const float deltaTime)
{
    constexpr int collisionSteps = 10;
    physicsSystem->Update(deltaTime, collisionSteps, tempAllocator, jobSystem);

    updateTransforms();
}

JPH::BodyInterface& Physics::getBodyInterface() const
{
    return physicsSystem->GetBodyInterface();
}

JPH::BodyID Physics::addRigidBody(IPhysicsBody* pb, const JPH::ShapeRefC& shape, const bool isDynamic)
{
    PhysicsObject physicsObject;
    physicsObject.physicsBody = pb;

    const JPH::BodyCreationSettings settings(
        shape,
        physics_utils::ToJolt(pb->getGlobalPosition()),
        physics_utils::ToJolt(pb->getGlobalRotation()),
        isDynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
        isDynamic ? Layers::MOVING : Layers::NON_MOVING
    );

    physicsObject.bodyId = physicsSystem->GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
    physicsObject.shape = shape;
    physicsObjects.insert({physicsObject.bodyId, physicsObject});

    return physicsObject.bodyId;
}

JPH::BodyID Physics::addRigidBody(IPhysicsBody* pb, const JPH::BodyCreationSettings& settings)
{
    PhysicsObject physicsObject;
    physicsObject.physicsBody = pb;
    physicsObject.bodyId = physicsSystem->GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
    physicsObject.shape = settings.GetShape();
    physicsObjects.insert({physicsObject.bodyId, physicsObject});

    return physicsObject.bodyId;
}

void Physics::removeRigidBody(const IPhysicsBody* pb)
{
    if (!physicsObjects.contains(pb->getBodyId())) { return; }

    const JPH::BodyID bodyId = physicsObjects[pb->getBodyId()].bodyId;
    physicsSystem->GetBodyInterface().RemoveBody(bodyId);
    physicsSystem->GetBodyInterface().DestroyBody(bodyId);
    physicsObjects.erase(pb->getBodyId());
}

void Physics::removeRigidBodies(const std::vector<IPhysicsBody*>& objects)
{
    std::vector<JPH::BodyID> bodyIDs;
    bodyIDs.reserve(objects.size());

    for (const auto& pb : objects) {
        const auto bodyId = pb->getBodyId();
        if (physicsObjects.contains(bodyId)) {
            bodyIDs.push_back(bodyId);
            physicsObjects.erase(bodyId);
        }
    }

    // Batch remove and destroy the bodies (Required by Jolt)
    if (!bodyIDs.empty()) {
        auto& bodyInterface = physicsSystem->GetBodyInterface();
        bodyInterface.RemoveBodies(bodyIDs.data(), bodyIDs.size());
        bodyInterface.DestroyBodies(bodyIDs.data(), bodyIDs.size());
    }
}

void Physics::updateTransforms() const
{
    const JPH::BodyInterface& bodyInterface = getBodyInterface();
    for (auto physicsObject : physicsObjects) {
        const JPH::Vec3 position = bodyInterface.GetPosition(physicsObject.first);
        const JPH::Quat rotation = bodyInterface.GetRotation(physicsObject.first);
        physicsObject.second.physicsBody->setGlobalTransformFromPhysics(physics_utils::ToGLM(position), physics_utils::ToGLM(rotation));
    }
}

IPhysicsBody* Physics::getPhysicsBodyFromId(JPH::BodyID bodyId)
{
    if (!physicsObjects.contains(bodyId)) { return nullptr; }
    return physicsObjects[bodyId].physicsBody;
}
}
