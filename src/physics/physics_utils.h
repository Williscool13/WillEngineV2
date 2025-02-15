//
// Created by William on 2024-12-13.
//

#ifndef PHYSICS_UTILS_H
#define PHYSICS_UTILS_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "physics_types.h"
#include "Jolt/Physics/Collision/CastResult.h"
#include "Jolt/Physics/Collision/RayCast.h"
#include "src/physics/physics.h"

namespace will_engine::physics
{
class PhysicsUtils
{
public:
    static glm::vec3 toGLM(const JPH::Vec3& vec);

    static glm::quat toGLM(const JPH::Quat& quat);

    static glm::mat4 toGLM(const JPH::Mat44& mat);

    static JPH::Vec3 toJolt(const glm::vec3& v);

    static JPH::Quat toJolt(const glm::quat& q);

    static bool raycastBroad(const glm::vec3& start, const glm::vec3& end);

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
    static RaycastHit raycast(const glm::vec3& start, const glm::vec3& end, const JPH::BroadPhaseLayerFilter& broadLayerFilter = {}, const JPH::ObjectLayerFilter& objectLayerFilter = {},
                              const JPH::BodyFilter& bodyFilter = {});

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
    static RaycastHit raycast(const glm::vec3& start, const glm::vec3& direction, float distance, const JPH::BroadPhaseLayerFilter& broadLayerFilter = {},
                              const JPH::ObjectLayerFilter& objectLayerFilter = {}, const JPH::BodyFilter& bodyFilter = {});

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
    static std::vector<RaycastHit> raycastAll(const glm::vec3& start, const glm::vec3& end, int maxHits = 16, const JPH::BroadPhaseLayerFilter& broadLayerFilter = {},
                                              const JPH::ObjectLayerFilter& objectLayerFilter = {}, const JPH::BodyFilter& bodyFilter = {}, const JPH::RayCastSettings& raycastSettings = {});

    static void addForce(const uint32_t bodyId, const glm::vec3& force) { addForce(JPH::BodyID(bodyId), force); }
    static void addForce(JPH::BodyID bodyId, const glm::vec3& force);

    static void addForceAtPosition(JPH::BodyID bodyId, const glm::vec3& force, const glm::vec3& position);

    static void addTorque(JPH::BodyID bodyId, const glm::vec3& torque);

    static void addImpulse(JPH::BodyID bodyId, const glm::vec3& impulse);

    static void addImpulseAtPosition(JPH::BodyID bodyId, const glm::vec3& impulse, const glm::vec3& position);

    static void addAngularImpulse(JPH::BodyID bodyId, const glm::vec3& angularImpulse);

    static void setLinearVelocity(JPH::BodyID bodyId, const glm::vec3& velocity);

    static void setAngularVelocity(JPH::BodyID bodyId, const glm::vec3& angularVelocity);

    static glm::vec3 getLinearVelocity(JPH::BodyID bodyId);

    static glm::vec3 getAngularVelocity(JPH::BodyID bodyId);

    static void resetVelocity(JPH::BodyID bodyId);

    static void setMotionType(JPH::BodyID bodyId, JPH::EMotionType motionType, JPH::EActivation activation);

    static JPH::ObjectLayer getObjectLayer(JPH::BodyID bodyId);

    static JPH::EShapeSubType getObjectShapeSubtype(JPH::BodyID bodyId);

    //inline RaycastHit sphereCast(const glm::vec3& start, const glm::vec3& end, float radius);
    //inline RaycastHit boxCast(const glm::vec3& start, const glm::vec3& end, const glm::vec3& halfExtents);
    // inline std::vector<JPH::BodyID> overlapSphere(const glm::vec3& center, float radius);
    // inline std::vector<JPH::BodyID> overlapBox(const glm::vec3& center, const glm::vec3& halfExtents);
    // inline bool isPointInShape(const glm::vec3& point, JPH::BodyID bodyId);
    // inline std::vector<JPH::BodyID> getBodiesTouchingPoint(const glm::vec3& point);
    // struct DistanceHit {
    //     float distance;
    //     glm::vec3 pointOnBody;
    //     JPH::BodyID. bodyId;
    // };
    // inline DistanceHit getClosestBody(const glm::vec3& point);
};
};


#endif //PHYSICS_UTILS_H
