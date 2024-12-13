//
// Created by William on 2024-12-13.
//

#ifndef PHYSICS_UTILS_H
#define PHYSICS_UTILS_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "src/physics/physics.h"

namespace physics_utils
{
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
}


#endif //PHYSICS_UTILS_H
