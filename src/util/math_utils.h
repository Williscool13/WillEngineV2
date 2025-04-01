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

inline uint32_t as_uint(const float x)
{
    return *(uint32_t*) &x;
}

inline float as_float(uint32_t x)
{
    return *(float*) &x;
}

/**
 * https://stackoverflow.com/a/60047308
 * @param x
 * @return
 */
inline float halfToFloat(const uint16_t x)
{
    // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
    const uint32_t e = (x & 0x7C00) >> 10; // exponent
    const uint32_t m = (x & 0x03FF) << 13; // mantissa
    const uint32_t v = as_uint((float) m) >> 23; // evil log2 bit hack to count leading zeros in denormalized format
    return as_float(
        (x & 0x8000) << 16 | (e != 0) * ((e + 112) << 23 | m) | ((e == 0) & (m != 0)) * (
            (v - 37) << 23 | ((m << (150 - v)) & 0x007FE000))); // sign : normalized : denormalized
}
}

#endif //MATH_UTILS_H
