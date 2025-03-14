//
// Created by William on 2024-12-06.
//

#include "physics.h"

#include <ranges>
#include <thread>
#include <fmt/format.h>

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
#include "physics_body.h"


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
    unitCubeShape = new JPH::BoxShape(PhysicsUtils::toJolt(UNIT_CUBE));
    unitSphereShape = new JPH::SphereShape(UNIT_SPHERE.x);
    unitCapsuleShape = new JPH::CapsuleShape(UNIT_CAPSULE.x, UNIT_CAPSULE.y);
    unitCylinderShape = new JPH::CylinderShape(UNIT_CYLINDER.x, UNIT_CYLINDER.y);
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

    syncGameData();

    physicsSystem->Update(deltaTime, collisionSteps, tempAllocator, jobSystem);

    updateGameData();
}

JPH::BodyInterface& Physics::getBodyInterface() const
{
    return physicsSystem->GetBodyInterface();
}

void Physics::removeRigidBody(const IPhysicsBody* pb)
{
    const auto bodyId = pb->getPhysicsBodyId();
    if (!physicsObjects.contains(bodyId)) { return; }

    physicsSystem->GetBodyInterface().RemoveBody(bodyId);
    physicsSystem->GetBodyInterface().DestroyBody(bodyId);
    physicsObjects.erase(bodyId);
}

void Physics::removeRigidBodies(const std::vector<IPhysicsBody*>& objects)
{
    std::vector<JPH::BodyID> bodyIDs;
    bodyIDs.reserve(objects.size());

    for (const auto& pb : objects) {
        const auto _bodyId = pb->getPhysicsBodyId();
        auto bodyId = JPH::BodyID(_bodyId);
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

void Physics::syncGameData()
{
    JPH::BodyInterface& bodyInterface = physicsSystem->GetBodyInterface();
    for (auto val : physicsObjects | std::views::values) {
        if (!val.physicsBody->isTransformDirty()) { continue; }

        glm::vec3 position = val.physicsBody->getGlobalPosition();
        glm::quat rotation = val.physicsBody->getGlobalRotation();

        bodyInterface.SetPosition(val.bodyId, PhysicsUtils::toJolt(position), JPH::EActivation::Activate);
        bodyInterface.SetRotation(val.bodyId, PhysicsUtils::toJolt(rotation), JPH::EActivation::Activate);
        val.physicsBody->undirty();
    }
}

void Physics::updateGameData() const
{
    const JPH::BodyInterface& bodyInterface = physicsSystem->GetBodyInterface();
    for (auto physicsObject : physicsObjects) {
        const JPH::Vec3 position = bodyInterface.GetPosition(physicsObject.first);
        const JPH::Quat rotation = bodyInterface.GetRotation(physicsObject.first);
        physicsObject.second.physicsBody->setTransform(PhysicsUtils::toGLM(position), PhysicsUtils::toGLM(rotation));
    }
}

PhysicsObject* Physics::getPhysicsObject(const JPH::BodyID bodyId)
{
    if (physicsObjects.contains(bodyId)) {
        return &physicsObjects.at(bodyId);
    }

    return nullptr;
}

JPH::BodyID Physics::setupRigidbody(IPhysicsBody* physicsBody, const JPH::EShapeSubType shapeType, const glm::vec3 shapeParams, const JPH::EMotionType motion, const JPH::ObjectLayer layer)
{
    if (physicsBody == nullptr) { return JPH::BodyID(JPH::BodyID::cMaxBodyIndex); }

    auto index = physicsBody->getPhysicsBodyId().GetIndex();
    if (index != JPH::BodyID::cMaxBodyIndex) {
        fmt::print("IPhysicsBody already has a rigidbody, failed to setup a new one\n");
        return physicsBody->getPhysicsBodyId();
    }

    JPH::ShapeRefC shape;
    switch (shapeType) {
        case JPH::EShapeSubType::Box:
            if (shapeParams == UNIT_CUBE) {
                shape = getUnitCubeShape();
                break;
            }
            shape = new JPH::BoxShape(PhysicsUtils::toJolt(shapeParams));
            break;
        case JPH::EShapeSubType::Sphere:
            if (shapeParams == UNIT_SPHERE) {
                shape = getUnitSphereShape();
                break;
            }

            shape = new JPH::SphereShape(shapeParams.x);
            break;
        case JPH::EShapeSubType::Capsule:
            if (shapeParams == UNIT_CAPSULE) {
                shape = getUnitCapsuleShape();
                break;
            }
            shape = new JPH::CapsuleShape(shapeParams.y, shapeParams.z);
            break;
        case JPH::EShapeSubType::Cylinder:
            if (shapeParams == UNIT_CYLINDER) {
                shape = getUnitCylinderShape();
                break;
            }
            shape = new JPH::CylinderShape(shapeParams.x, shapeParams.y, shapeParams.z);
            break;
        default:
            fmt::print("Collider type not yet supported");
            return JPH::BodyID(JPH::BodyID::cMaxBodyIndex);
    }

    const JPH::BodyCreationSettings settings{
        shape,
        PhysicsUtils::toJolt(physicsBody->getGlobalPosition()),
        PhysicsUtils::toJolt(physicsBody->getGlobalRotation()),
        motion,
        layer
    };

    PhysicsObject physicsObject;
    physicsObject.physicsBody = physicsBody;
    physicsObject.bodyId = physicsSystem->GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
    physicsObject.shape = shape;

    physicsObjects.insert({physicsObject.bodyId, physicsObject});

    physicsBody->setPhysicsBodyId(physicsObject.bodyId);
    return physicsObject.bodyId;
}

JPH::BodyID Physics::setupRigidbody(IPhysicsBody* physicsBody, JPH::HeightFieldShapeSettings& heightFieldShapeSettings, JPH::EMotionType motion, JPH::ObjectLayer layer)
{
    const JPH::ShapeSettings::ShapeResult result = heightFieldShapeSettings.Create();
    if (!result.IsValid()) {
        fmt::print("Failed to create terrain collision shape: {}\n", result.GetError());
        assert(false);
        return JPH::BodyID(JPH::BodyID::cMaxBodyIndex);
    }

    const JPH::ShapeRefC terrainShape = result.Get();
    JPH::BodyCreationSettings bodySettings(
        terrainShape,
        PhysicsUtils::toJolt(glm::vec3(0.0f)),
        PhysicsUtils::toJolt(glm::quat{1.0f, 0.0f, 0.0f, 0.0f}),
        JPH::EMotionType::Static,
        Layers::TERRAIN
    );

    const JPH::BodyCreationSettings settings{
        terrainShape,
        PhysicsUtils::toJolt(physicsBody->getGlobalPosition()),
        PhysicsUtils::toJolt(physicsBody->getGlobalRotation()),
        motion,
        layer
    };

    PhysicsObject physicsObject;
    physicsObject.physicsBody = physicsBody;
    physicsObject.bodyId = physicsSystem->GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
    physicsObject.shape = terrainShape;

    physicsObjects.insert({physicsObject.bodyId, physicsObject});

    physicsBody->setPhysicsBodyId(physicsObject.bodyId);
    return physicsObject.bodyId;
}

void Physics::releaseRigidbody(IPhysicsBody* physicsBody)
{
    const auto bodyId = physicsBody->getPhysicsBodyId();
    if (bodyId.GetIndex() == JPH::BodyID::cMaxBodyIndex) { return; }
    if (!physicsObjects.contains(bodyId)) { return; }

    physicsSystem->GetBodyInterface().RemoveBody(bodyId);
    physicsSystem->GetBodyInterface().DestroyBody(bodyId);
    physicsObjects.erase(bodyId);
    physicsBody->setPhysicsBodyId(JPH::BodyID(JPH::BodyID::cMaxBodyIndex));
}

void Physics::setPositionAndRotation(const JPH::BodyID bodyId, const glm::vec3 position, const glm::quat rotation, const bool activate) const
{
    if (!physicsObjects.contains(bodyId)) {
        fmt::print("Failed to set physics position and rotation (body ID not found)\n");
        return;
    }

    physicsSystem->GetBodyInterface().SetPosition(bodyId, PhysicsUtils::toJolt(position), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    physicsSystem->GetBodyInterface().SetRotation(bodyId, PhysicsUtils::toJolt(rotation), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
}

void Physics::setPosition(const JPH::BodyID bodyId, const glm::vec3 position, const bool activate) const
{
    if (!physicsObjects.contains(bodyId)) {
        fmt::print("Failed to set physics position (body ID not found)\n");
        return;
    }

    physicsSystem->GetBodyInterface().SetPosition(bodyId, PhysicsUtils::toJolt(position), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
}

void Physics::setRotation(const JPH::BodyID bodyId, const glm::quat rotation, const bool activate) const
{
    if (!physicsObjects.contains(bodyId)) {
        fmt::print("Failed to set physics rotation (body ID not found)\n");
        return;
    }

    physicsSystem->GetBodyInterface().SetRotation(bodyId, PhysicsUtils::toJolt(rotation), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
}

void Physics::setPositionAndRotation(const JPH::BodyID bodyId, const JPH::Vec3 position, const JPH::Quat rotation, const bool activate) const
{
    if (!physicsObjects.contains(bodyId)) { return; }

    physicsSystem->GetBodyInterface().SetPosition(bodyId, position, activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    physicsSystem->GetBodyInterface().SetRotation(bodyId, rotation, activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
}

void Physics::setPosition(const JPH::BodyID bodyId, const JPH::Vec3 position, const bool activate) const
{
    if (!physicsObjects.contains(bodyId)) { return; }

    physicsSystem->GetBodyInterface().SetPosition(bodyId, position, activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
}

void Physics::setRotation(const JPH::BodyID bodyId, const JPH::Quat rotation, const bool activate) const
{
    if (!physicsObjects.contains(bodyId)) { return; }

    physicsSystem->GetBodyInterface().SetRotation(bodyId, rotation, activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
}

JPH::EMotionType Physics::getMotionType(const IPhysicsBody* body) const
{
    return physicsSystem->GetBodyInterface().GetMotionType(body->getPhysicsBodyId());
}

JPH::ObjectLayer Physics::getLayer(const IPhysicsBody* body) const
{
    return physicsSystem->GetBodyInterface().GetObjectLayer(body->getPhysicsBodyId());
}

void Physics::setMotionType(const IPhysicsBody* body, const JPH::EMotionType motionType, const JPH::EActivation activation) const
{
    physicsSystem->GetBodyInterface().SetMotionType(body->getPhysicsBodyId(), motionType, activation);
}

PhysicsProperties Physics::serializeProperties(const IPhysicsBody* physicsBody) const
{
    if (physicsBody == nullptr || physicsBody->getPhysicsBodyId().GetIndex() == JPH::BodyID::cMaxBodyIndex) {
        return {false};
    }
    const auto bodyId = JPH::BodyID(physicsBody->getPhysicsBodyId());
    const PhysicsObject* physicsObject = physics->getPhysicsObject(bodyId);
    if (physicsObject == nullptr) {
        return {false};
    }
    PhysicsProperties properties{true};
    properties.motionType = static_cast<uint8_t>(physicsSystem->GetBodyInterface().GetMotionType(bodyId));
    properties.layer = physicsSystem->GetBodyInterface().GetObjectLayer(bodyId);


    const JPH::ShapeRefC shape = physicsObject->shape;
    properties.shapeType = shape->GetSubType();

    switch (shape->GetSubType()) {
        case JPH::EShapeSubType::Box:
        {
            const auto box = static_cast<const JPH::BoxShape*>(shape.GetPtr());
            const JPH::Vec3 halfExtent = box->GetHalfExtent();
            properties.shapeParams = glm::vec3(halfExtent.GetX(), halfExtent.GetY(), halfExtent.GetZ());
            break;
        }
        case JPH::EShapeSubType::Sphere:
        {
            const auto sphere = static_cast<const JPH::SphereShape*>(shape.GetPtr());
            properties.shapeParams = glm::vec3(sphere->GetRadius(), 0.0f, 0.0f);
            break;
        }
        case JPH::EShapeSubType::Capsule:
        {
            const auto capsule = static_cast<const JPH::CapsuleShape*>(shape.GetPtr());
            properties.shapeParams = glm::vec3(capsule->GetRadius(), capsule->GetHalfHeightOfCylinder(), 0.0f);
            break;
        }
        case JPH::EShapeSubType::Cylinder:
        {
            const auto cylinder = static_cast<const JPH::CylinderShape*>(shape.GetPtr());
            properties.shapeParams = glm::vec3(cylinder->GetRadius(), cylinder->GetHalfHeight(), 0.0f);
            break;
        }
        default:
            fmt::print("Warning: Unsupported physics shape subtype: {}\n", static_cast<int>(shape->GetSubType()));
            break;
    }

    return properties;
}

bool Physics::deserializeProperties(IPhysicsBody* physicsBody, const PhysicsProperties& properties)
{
    if (!properties.isActive || physicsBody == nullptr) {
        return false;
    }

    auto bodyId = setupRigidbody(physicsBody, properties.shapeType, properties.shapeParams, static_cast<JPH::EMotionType>(properties.motionType), properties.layer);
    return bodyId.GetIndex() != JPH::BodyID::cMaxBodyIndex;
}
}
