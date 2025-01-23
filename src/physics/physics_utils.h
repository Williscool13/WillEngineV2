//
// Created by William on 2024-12-13.
//

#ifndef PHYSICS_UTILS_H
#define PHYSICS_UTILS_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "physics_types.h"
#include "Jolt/Physics/Collision/CastResult.h"
#include "Jolt/Physics/Collision/CollisionCollectorImpl.h"
#include "Jolt/Physics/Collision/RayCast.h"
#include "src/physics/physics.h"

namespace will_engine::physics::physics_utils
{
inline glm::vec3 ToGLM(const JPH::Vec3& vec)
{
    return {vec.GetX(), vec.GetY(), vec.GetZ()};
}

inline glm::quat ToGLM(const JPH::Quat& quat)
{
    return {quat.GetW(), quat.GetX(), quat.GetY(), quat.GetZ()};
}

inline glm::mat4 ToGLM(const JPH::Mat44& mat)
{
    return {
        mat.GetColumn4(0).GetX(), mat.GetColumn4(0).GetY(), mat.GetColumn4(0).GetZ(), mat.GetColumn4(0).GetW(),
        mat.GetColumn4(1).GetX(), mat.GetColumn4(1).GetY(), mat.GetColumn4(1).GetZ(), mat.GetColumn4(1).GetW(),
        mat.GetColumn4(2).GetX(), mat.GetColumn4(2).GetY(), mat.GetColumn4(2).GetZ(), mat.GetColumn4(2).GetW(),
        mat.GetColumn4(3).GetX(), mat.GetColumn4(3).GetY(), mat.GetColumn4(3).GetZ(), mat.GetColumn4(3).GetW()
    };
}

inline JPH::Vec3 ToJolt(const glm::vec3& v)
{
    return {v.x, v.y, v.z};
}

inline JPH::Quat ToJolt(const glm::quat& q)
{
    return {q.x, q.y, q.z, q.w};
}

inline IPhysicsBody* getPhysicsBodyFromId(const JPH::BodyID bodyId)
{
    Physics* physics = Physics::Get();
    if (!physics) return nullptr;

    return physics->getPhysicsBodyFromId(bodyId);
}

inline bool raycastBroad(const glm::vec3& start, const glm::vec3& end)
{
    if (!Physics::Get()) return false;

    const JPH::RVec3 rayStart = ToJolt(start);
    const JPH::RVec3 rayEnd = ToJolt(end);
    const JPH::RayCast ray{rayStart, rayEnd - rayStart};

    JPH::AllHitCollisionCollector<JPH::RayCastBodyCollector> collector;
    Physics::Get()->getPhysicsSystem().GetBroadPhaseQuery().CastRay(ray, collector);

    return collector.HadHit();
}

/**
     * Casts a ray and find the closest hit.
     * \n Returns a \code RaycastHit\endcode struct that details the details of the hit.
     * @param start
     * @param end
     * @param broadLayerFilter
     * @param objectLayerFilter
     * @param bodyFilter
     * @return
     */
inline RaycastHit raycast(const glm::vec3& start, const glm::vec3& end, const JPH::BroadPhaseLayerFilter& broadLayerFilter = {}, const JPH::ObjectLayerFilter& objectLayerFilter = {},
                          const JPH::BodyFilter& bodyFilter = {})
{
    RaycastHit result;
    if (!Physics::Get()) return result;

    const JPH::RVec3 rayStart = ToJolt(start);
    const JPH::RVec3 rayEnd = ToJolt(end);
    const JPH::RRayCast ray{rayStart, rayEnd - rayStart};

    JPH::RayCastResult hit;
    if (Physics::Get()->getPhysicsSystem().GetNarrowPhaseQuery().CastRay(ray, hit, broadLayerFilter, objectLayerFilter, bodyFilter)) {
        result.hasHit = true;
        result.fraction = hit.mFraction;
        result.hitPosition = start + (end - start) * hit.mFraction;
        result.distance = glm::distance(start, result.hitPosition);
        result.hitBodyID = hit.mBodyID;
        result.subShapeID = hit.mSubShapeID2;

        // Get hit normal
        const JPH::BodyLockRead lock(Physics::Get()->getPhysicsSystem().GetBodyLockInterface(), hit.mBodyID);
        if (lock.Succeeded()) {
            const JPH::Body& body = lock.GetBody();
            const JPH::Vec3 normal = body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, ToJolt(result.hitPosition));
            result.hitNormal = glm::vec3(normal.GetX(), normal.GetY(), normal.GetZ());
        }
    }

    return result;
}

/**
     * Casts a ray and find the closest hit.
     * \n Returns a \code RaycastHit\endcode struct that details the details of the hit.
     * @param start
     * @param direction
     * @param distance
     * @param broadLayerFilter
     * @param objectLayerFilter
     * @param bodyFilter
     * @return
     */
inline RaycastHit raycast(const glm::vec3& start, const glm::vec3& direction, const float distance, const JPH::BroadPhaseLayerFilter& broadLayerFilter = {},
                          const JPH::ObjectLayerFilter& objectLayerFilter = {}, const JPH::BodyFilter& bodyFilter = {})
{
    const glm::vec3 normalizedDir = glm::normalize(direction);
    const glm::vec3 end = start + normalizedDir * distance;
    return raycast(start, end, broadLayerFilter, objectLayerFilter, bodyFilter);
}

/**
     * Casts a ray and finds \code maxHits\endcode number of physics object intersections.
     * \n Returns a vector of \code RaycastHit\endcode structs that details the details of the hits.
     * @param start
     * @param end
     * @param maxHits
     * @param broadLayerFilter
     * @param objectLayerFilter
     * @param bodyFilter
     * @param raycastSettings
     * @return
     */
inline std::vector<RaycastHit> raycastAll(const glm::vec3& start, const glm::vec3& end, const int maxHits = 16, const JPH::BroadPhaseLayerFilter& broadLayerFilter = {},
                                          const JPH::ObjectLayerFilter& objectLayerFilter = {}, const JPH::BodyFilter& bodyFilter = {}, const JPH::RayCastSettings& raycastSettings = {})
{
    std::vector<RaycastHit> results;
    if (!Physics::Get()) return results;

    const JPH::RVec3 rayStart = ToJolt(start);
    const JPH::RVec3 rayEnd = ToJolt(end);
    const JPH::RRayCast ray{rayStart, rayEnd - rayStart};


    JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
    Physics::Get()->getPhysicsSystem().GetNarrowPhaseQuery().CastRay(ray, raycastSettings, collector, broadLayerFilter, objectLayerFilter, bodyFilter);

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

        const JPH::BodyLockRead lock(Physics::Get()->getPhysicsSystem().GetBodyLockInterface(), hit.mBodyID);
        if (lock.Succeeded()) {
            const JPH::Body& body = lock.GetBody();
            JPH::Vec3 normal = body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, ToJolt(result.hitPosition));
            result.hitNormal = glm::vec3(normal.GetX(), normal.GetY(), normal.GetZ());
        }

        results.push_back(result);
    }

    return results;
}

inline void addForce(const IPhysicsBody* physicsBody, const glm::vec3& force)
{
    if (!Physics::Get()) return;
    if (!Physics::Get()->doesPhysicsBodyExists(physicsBody->getBodyId())) { return; }
    Physics::Get()->getBodyInterface().AddForce(physicsBody->getBodyId(), ToJolt(force));
}

inline void addForce(const JPH::BodyID bodyId, const glm::vec3& force)
{
    if (!Physics::Get()) return;
    if (!Physics::Get()->doesPhysicsBodyExists(bodyId)) { return; }

    Physics::Get()->getBodyInterface().AddForce(bodyId, ToJolt(force));
}

inline void addForceAtPosition(const JPH::BodyID bodyId, const glm::vec3& force, const glm::vec3& position)
{
    if (!Physics::Get()) return;
    Physics::Get()->getBodyInterface().AddForce(bodyId, ToJolt(force), ToJolt(position));
}

inline void addTorque(JPH::BodyID bodyId, const glm::vec3& torque)
{
    if (!Physics::Get()) return;
    Physics::Get()->getBodyInterface().AddTorque(bodyId, ToJolt(torque));
}

inline void addImpulse(JPH::BodyID bodyId, const glm::vec3& impulse)
{
    if (!Physics::Get()) return;
    Physics::Get()->getBodyInterface().AddImpulse(bodyId, ToJolt(impulse));
}

inline void addImpulseAtPosition(JPH::BodyID bodyId, const glm::vec3& impulse, const glm::vec3& position)
{
    if (!Physics::Get()) return;
    Physics::Get()->getBodyInterface().AddImpulse(bodyId, ToJolt(impulse), ToJolt(position));
}

inline void addAngularImpulse(JPH::BodyID bodyId, const glm::vec3& angularImpulse)
{
    if (!Physics::Get()) return;
    Physics::Get()->getBodyInterface().AddAngularImpulse(bodyId, ToJolt(angularImpulse));
}

inline void setLinearVelocity(JPH::BodyID bodyId, const glm::vec3& velocity)
{
    if (!Physics::Get()) return;
    Physics::Get()->getBodyInterface().SetLinearVelocity(bodyId, ToJolt(velocity));
}

inline void setAngularVelocity(JPH::BodyID bodyId, const glm::vec3& angularVelocity)
{
    if (!Physics::Get()) return;
    Physics::Get()->getBodyInterface().SetAngularVelocity(bodyId, ToJolt(angularVelocity));
}

inline glm::vec3 getLinearVelocity(JPH::BodyID bodyId)
{
    if (!Physics::Get()) return glm::vec3(0.0f);
    const JPH::Vec3 velocity = Physics::Get()->getBodyInterface().GetLinearVelocity(bodyId);
    return glm::vec3(velocity.GetX(), velocity.GetY(), velocity.GetZ());
}

inline glm::vec3 getAngularVelocity(JPH::BodyID bodyId)
{
    if (!Physics::Get()) return glm::vec3(0.0f);
    const JPH::Vec3 velocity = Physics::Get()->getBodyInterface().GetAngularVelocity(bodyId);
    return glm::vec3(velocity.GetX(), velocity.GetY(), velocity.GetZ());
}

//inline RaycastHit sphereCast(const glm::vec3& start, const glm::vec3& end, float radius);
//inline RaycastHit boxCast(const glm::vec3& start, const glm::vec3& end, const glm::vec3& halfExtents);
// inline std::vector<JPH::BodyID> overlapSphere(const glm::vec3& center, float radius);
// inline std::vector<JPH::BodyID> overlapBox(const glm::vec3& center, const glm::vec3& halfExtents);
// inline bool isPointInShape(const glm::vec3& point, JPH::BodyID bodyId);
// inline std::vector<JPH::BodyID> getBodiesTouchingPoint(const glm::vec3& point);
// struct DistanceHit {
//     float distance;
//     glm::vec3 pointOnBody;
//     JPH::BodyID bodyId;
// };
// inline DistanceHit getClosestBody(const glm::vec3& point);
}


#endif //PHYSICS_UTILS_H
