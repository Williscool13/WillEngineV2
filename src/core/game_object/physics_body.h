//
// Created by William on 2025-01-23.
//

#ifndef PHYSICS_BODY_H
#define PHYSICS_BODY_H

#include <glm/glm.hpp>

namespace will_engine
{
class IPhysicsBody
{
public:
    virtual ~IPhysicsBody() = default;

    virtual void setGlobalTransformFromPhysics(const glm::vec3& position, const glm::quat& rotation) = 0;

    virtual glm::vec3 getGlobalPosition() = 0;

    virtual glm::quat getGlobalRotation() = 0;

    virtual void setBodyId(JPH::BodyID bodyId) = 0;

    virtual JPH::BodyID getBodyId() const = 0;
};
}

#endif //PHYSICS_BODY_H
