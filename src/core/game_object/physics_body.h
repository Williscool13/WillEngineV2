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

    virtual void setTransform(const glm::vec3& position, const glm::quat& rotation) = 0;

    /**
     * Gets global position of the physics body
     * @return
     */
    virtual glm::vec3 getPosition() = 0;

    /**
     * Gets global rotation of the physics body
     * @return
     */
    virtual glm::quat getRotation() = 0;

    virtual void setBodyId(JPH::BodyID bodyId) = 0;

    virtual JPH::BodyID getBodyId() const = 0;
};
}

#endif //PHYSICS_BODY_H
