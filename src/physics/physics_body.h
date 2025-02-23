//
// Created by William on 2025-01-23.
//

#ifndef PHYSICS_BODY_H
#define PHYSICS_BODY_H

#include <glm/glm.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/MotionType.h>

namespace will_engine
{
class IPhysicsBody
{
public:
    virtual ~IPhysicsBody() = default;

    virtual void setGameTransformFromPhysics(const glm::vec3& position, const glm::quat& rotation) = 0;

    virtual void setPhysicsTransformFromGame(const glm::vec3& position, const glm::quat& rotation) = 0;

    virtual glm::vec3 getGlobalPosition() = 0;

    virtual glm::quat getGlobalRotation() = 0;

    virtual void setPhysicsBodyId(JPH::BodyID bodyId) = 0;

    [[nodiscard]] virtual JPH::BodyID getPhysicsBodyId() const = 0;
};
}

#endif //PHYSICS_BODY_H
