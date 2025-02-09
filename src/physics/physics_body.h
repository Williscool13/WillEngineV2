//
// Created by William on 2025-01-23.
//

#ifndef PHYSICS_BODY_H
#define PHYSICS_BODY_H

#include <glm.hpp>

namespace will_engine
{
class IPhysicsBody
{
public:
    virtual ~IPhysicsBody() = default;

    virtual void setGlobalTransformFromPhysics(const glm::vec3& position, const glm::quat& rotation) = 0;

    virtual glm::vec3 getGlobalPosition() = 0;

    virtual glm::quat getGlobalRotation() = 0;

    virtual void setPhysicsBodyId(uint32_t bodyId) = 0;

    [[nodiscard]] virtual uint32_t getPhysicsBodyId() const = 0;
};
}

#endif //PHYSICS_BODY_H
