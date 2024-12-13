//
// Created by William on 2024-12-13.
//

#ifndef PHYSICS_BODY_H
#define PHYSICS_BODY_H

#include "src/core/game_object.h"
#include "src/physics/physics.h"

class PhysicsBody
{
public:
    PhysicsBody() = default;

    void updateTransform(const JPH::BodyInterface& bodyInterface) const
    {
        if (!gameObject) return;

        const JPH::Vec3 position = bodyInterface.GetPosition(bodyId);
        gameObject->transform.setPosition(glm::vec3(position.GetX(), position.GetY(), position.GetZ()));

        // Get rotation as quaternion
        const JPH::Quat rotation = bodyInterface.GetRotation(bodyId);
        gameObject->transform.setRotation(glm::quat(rotation.GetW(), rotation.GetX(), rotation.GetY(), rotation.GetZ()));

        gameObject->dirty();
    }

    GameObject* gameObject = nullptr;
    JPH::BodyID bodyId;
    JPH::ShapeRefC shape = nullptr;
};

#endif //PHYSICS_BODY_H
