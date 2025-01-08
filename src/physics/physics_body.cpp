//
// Created by William on 2024-12-13.
//

#include "physics_body.h"

#include <glm/glm.hpp>

#include "src/core/game_object.h"

void PhysicsBody::updateTransform(const JPH::BodyInterface& bodyInterface) const
{
    if (!gameObject) return;

    const JPH::Vec3 position = bodyInterface.GetPosition(bodyId);
    const JPH::Quat rotation = bodyInterface.GetRotation(bodyId);
    gameObject->setGlobalPosition(glm::vec3(position.GetX(), position.GetY(), position.GetZ()));
    gameObject->setGlobalRotation(glm::quat(rotation.GetW(), rotation.GetX(), rotation.GetY(), rotation.GetZ()));
}
