//
// Created by William on 2024-12-13.
//

#ifndef PHYSICS_BODY_H
#define PHYSICS_BODY_H

#include "src/physics/physics.h"

class PhysicsBody
{
public:
    PhysicsBody() = default;

    void updateTransform(const JPH::BodyInterface& bodyInterface) const;

    GameObject* gameObject = nullptr;
    JPH::BodyID bodyId;
    JPH::ShapeRefC shape = nullptr;
};

#endif //PHYSICS_BODY_H
