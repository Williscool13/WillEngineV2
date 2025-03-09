//
// Created by William on 2025-02-23.
//

#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace will_engine::math
{
inline void decomposeMatrix(const glm::mat4& matrix, glm::vec3& position, glm::quat& rotation, glm::vec3& scale)
{
    position = glm::vec3(matrix[3]);

    scale.x = length(glm::vec3(matrix[0]));
    scale.y = length(glm::vec3(matrix[1]));
    scale.z = length(glm::vec3(matrix[2]));

    const glm::mat3 rotMat(
        glm::vec3(matrix[0]) / scale.x,
        glm::vec3(matrix[1]) / scale.y,
        glm::vec3(matrix[2]) / scale.z
    );
    rotation = quat_cast(rotMat);
}
}

#endif //MATH_UTILS_H
