//
// Created by William on 2025-02-08.
//

#include "physics_utils.h"

#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>

glm::vec3 will_engine::physics::PhysicsUtils::toGLM(const JPH::Vec3& vec)
{
    return {vec.GetX(), vec.GetY(), vec.GetZ()};
}

glm::quat will_engine::physics::PhysicsUtils::toGLM(const JPH::Quat& quat)
{
    return {quat.GetW(), quat.GetX(), quat.GetY(), quat.GetZ()};
}

glm::mat4 will_engine::physics::PhysicsUtils::toGLM(const JPH::Mat44& mat)
{
    return {
        mat.GetColumn4(0).GetX(), mat.GetColumn4(0).GetY(), mat.GetColumn4(0).GetZ(), mat.GetColumn4(0).GetW(),
        mat.GetColumn4(1).GetX(), mat.GetColumn4(1).GetY(), mat.GetColumn4(1).GetZ(), mat.GetColumn4(1).GetW(),
        mat.GetColumn4(2).GetX(), mat.GetColumn4(2).GetY(), mat.GetColumn4(2).GetZ(), mat.GetColumn4(2).GetW(),
        mat.GetColumn4(3).GetX(), mat.GetColumn4(3).GetY(), mat.GetColumn4(3).GetZ(), mat.GetColumn4(3).GetW()
    };
}

JPH::Vec3 will_engine::physics::PhysicsUtils::toJolt(const glm::vec3& v)
{
    return {v.x, v.y, v.z};
}

JPH::Quat will_engine::physics::PhysicsUtils::toJolt(const glm::quat& q)
{
    return {q.x, q.y, q.z, q.w};
}

bool will_engine::physics::PhysicsUtils::raycastBroad(const glm::vec3& start, const glm::vec3& end)
{
    if (!Physics::get()) return false;

    const JPH::RVec3 rayStart = toJolt(start);
    const JPH::RVec3 rayEnd = toJolt(end);
    const JPH::RayCast ray{rayStart, rayEnd - rayStart};

    JPH::AllHitCollisionCollector<JPH::RayCastBodyCollector> collector;
    Physics::get()->getPhysicsSystem().GetBroadPhaseQuery().CastRay(ray, collector);

    return collector.HadHit();
}

will_engine::physics::RaycastHit will_engine::physics::PhysicsUtils::raycast(const glm::vec3& start, const glm::vec3& end, const JPH::BroadPhaseLayerFilter& broadLayerFilter,
                                                                             const JPH::ObjectLayerFilter& objectLayerFilter, const JPH::BodyFilter& bodyFilter)
{
    RaycastHit result;
    if (!Physics::get()) return result;

    const JPH::RVec3 rayStart = toJolt(start);
    const JPH::RVec3 rayEnd = toJolt(end);
    const JPH::RRayCast ray{rayStart, rayEnd - rayStart};

    JPH::RayCastResult hit;
    if (Physics::get()->getPhysicsSystem().GetNarrowPhaseQuery().CastRay(ray, hit, broadLayerFilter, objectLayerFilter, bodyFilter)) {
        result.hasHit = true;
        result.fraction = hit.mFraction;
        result.hitPosition = start + (end - start) * hit.mFraction;
        result.distance = glm::distance(start, result.hitPosition);
        result.hitBodyID = hit.mBodyID;
        result.subShapeID = hit.mSubShapeID2;

        // Get hit normal
        const JPH::BodyLockRead lock(Physics::get()->getPhysicsSystem().GetBodyLockInterface(), hit.mBodyID);
        if (lock.Succeeded()) {
            const JPH::Body& body = lock.GetBody();
            const JPH::Vec3 normal = body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, toJolt(result.hitPosition));
            result.hitNormal = glm::vec3(normal.GetX(), normal.GetY(), normal.GetZ());
        }
    }

    return result;
}

will_engine::physics::RaycastHit will_engine::physics::PhysicsUtils::raycast(const glm::vec3& start, const glm::vec3& direction, const float distance,
                                                                             const JPH::BroadPhaseLayerFilter& broadLayerFilter, const JPH::ObjectLayerFilter& objectLayerFilter,
                                                                             const JPH::BodyFilter& bodyFilter)
{
    const glm::vec3 normalizedDir = glm::normalize(direction);
    const glm::vec3 end = start + normalizedDir * distance;
    return raycast(start, end, broadLayerFilter, objectLayerFilter, bodyFilter);
}

std::vector<will_engine::physics::RaycastHit> will_engine::physics::PhysicsUtils::raycastAll(const glm::vec3& start, const glm::vec3& end, const int maxHits,
                                                                                             const JPH::BroadPhaseLayerFilter& broadLayerFilter, const JPH::ObjectLayerFilter& objectLayerFilter,
                                                                                             const JPH::BodyFilter& bodyFilter, const JPH::RayCastSettings& raycastSettings)
{
    std::vector<RaycastHit> results;
    if (!Physics::get()) return results;

    const JPH::RVec3 rayStart = toJolt(start);
    const JPH::RVec3 rayEnd = toJolt(end);
    const JPH::RRayCast ray{rayStart, rayEnd - rayStart};


    JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
    Physics::get()->getPhysicsSystem().GetNarrowPhaseQuery().CastRay(ray, raycastSettings, collector, broadLayerFilter, objectLayerFilter, bodyFilter);

    collector.Sort();
    const auto& hits = collector.mHits;
    const size_t numHits = std::min(static_cast<size_t>(maxHits), hits.size());

    results.reserve(numHits);
    for (size_t i = 0; i < numHits; ++i) {
        const auto& hit = hits[i];
        RaycastHit result;
        result.hasHit = true;
        result.fraction = hit.mFraction;
        result.hitPosition = start + (end - start) * hit.mFraction;
        result.distance = glm::distance(start, result.hitPosition);
        result.hitBodyID = hit.mBodyID;
        result.subShapeID = hit.mSubShapeID2;

        const JPH::BodyLockRead lock(Physics::get()->getPhysicsSystem().GetBodyLockInterface(), hit.mBodyID);
        if (lock.Succeeded()) {
            const JPH::Body& body = lock.GetBody();
            JPH::Vec3 normal = body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, toJolt(result.hitPosition));
            result.hitNormal = glm::vec3(normal.GetX(), normal.GetY(), normal.GetZ());
        }

        results.push_back(result);
    }

    return results;
}

void will_engine::physics::PhysicsUtils::addForce(const JPH::BodyID bodyId, const glm::vec3& force)
{
    if (!Physics::get()) return;
    if (!Physics::get()->doesPhysicsBodyExists(bodyId)) { return; }

    Physics::get()->getBodyInterface().AddForce(bodyId, toJolt(force));
}

void will_engine::physics::PhysicsUtils::addForceAtPosition(const JPH::BodyID bodyId, const glm::vec3& force, const glm::vec3& position)
{
    if (!Physics::get()) return;
    Physics::get()->getBodyInterface().AddForce(bodyId, toJolt(force), toJolt(position));
}

void will_engine::physics::PhysicsUtils::addTorque(const JPH::BodyID bodyId, const glm::vec3& torque)
{
    if (!Physics::get()) return;
    Physics::get()->getBodyInterface().AddTorque(bodyId, toJolt(torque));
}

void will_engine::physics::PhysicsUtils::addImpulse(const JPH::BodyID bodyId, const glm::vec3& impulse)
{
    if (!Physics::get()) return;
    Physics::get()->getBodyInterface().AddImpulse(bodyId, toJolt(impulse));
}

void will_engine::physics::PhysicsUtils::addImpulseAtPosition(const JPH::BodyID bodyId, const glm::vec3& impulse, const glm::vec3& position)
{
    if (!Physics::get()) return;
    Physics::get()->getBodyInterface().AddImpulse(bodyId, toJolt(impulse), toJolt(position));
}

void will_engine::physics::PhysicsUtils::addAngularImpulse(const JPH::BodyID bodyId, const glm::vec3& angularImpulse)
{
    if (!Physics::get()) return;
    Physics::get()->getBodyInterface().AddAngularImpulse(bodyId, toJolt(angularImpulse));
}

void will_engine::physics::PhysicsUtils::setLinearVelocity(const JPH::BodyID bodyId, const glm::vec3& velocity)
{
    if (!Physics::get()) return;
    Physics::get()->getBodyInterface().SetLinearVelocity(bodyId, toJolt(velocity));
}

void will_engine::physics::PhysicsUtils::setAngularVelocity(const JPH::BodyID bodyId, const glm::vec3& angularVelocity)
{
    if (!Physics::get()) return;
    Physics::get()->getBodyInterface().SetAngularVelocity(bodyId, toJolt(angularVelocity));
}

glm::vec3 will_engine::physics::PhysicsUtils::getLinearVelocity(const JPH::BodyID bodyId)
{
    if (!Physics::get()) return glm::vec3(0.0f);
    const JPH::Vec3 velocity = Physics::get()->getBodyInterface().GetLinearVelocity(bodyId);
    return {velocity.GetX(), velocity.GetY(), velocity.GetZ()};
}

glm::vec3 will_engine::physics::PhysicsUtils::getAngularVelocity(const JPH::BodyID bodyId)
{
    if (!Physics::get()) return glm::vec3(0.0f);
    const JPH::Vec3 velocity = Physics::get()->getBodyInterface().GetAngularVelocity(bodyId);
    return {velocity.GetX(), velocity.GetY(), velocity.GetZ()};
}

void will_engine::physics::PhysicsUtils::resetVelocity(JPH::BodyID bodyId)
{
    setAngularVelocity(bodyId, glm::vec3(0.0f));
    setLinearVelocity(bodyId, glm::vec3(0.0f));
}

void will_engine::physics::PhysicsUtils::setMotionType(const JPH::BodyID bodyId, const JPH::EMotionType motionType, const JPH::EActivation activation)
{
    if (!Physics::get()) return;
    Physics::get()->getBodyInterface().SetMotionType(bodyId, motionType, activation);
}

JPH::ObjectLayer will_engine::physics::PhysicsUtils::getObjectLayer(const JPH::BodyID bodyId)
{
    if (!Physics::get()) return 0;
    return Physics::get()->getBodyInterface().GetObjectLayer(bodyId);
}



JPH::EShapeSubType will_engine::physics::PhysicsUtils::getObjectShapeSubtype(JPH::BodyID bodyId)
{
    if (!Physics::get()) return JPH::EShapeSubType::Empty;
    const JPH::RefConst<JPH::Shape> shape = Physics::get()->getBodyInterface().GetShape(bodyId);
    return shape->GetSubType();
}
