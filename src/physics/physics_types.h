//
// Created by William on 2024-12-15.
//

#ifndef PHYSICS_TYPES_H
#define PHYSICS_TYPES_H

#include <glm/glm.hpp>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include "Jolt/Physics/Collision/Shape/SubShapeID.h"

struct RaycastHit {
    bool hasHit = false;
    float fraction = 0.0f;
    float distance = 0.0f;
    glm::vec3 hitPosition{0.0f};
    glm::vec3 hitNormal{0.0f};
    JPH::BodyID hitBodyID;
    JPH::SubShapeID subShapeID;
};


#endif //PHYSICS_TYPES_H
