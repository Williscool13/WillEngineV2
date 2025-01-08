//
// Created by William on 8/11/2024.
//

#ifndef RENDER_UTILS_H
#define RENDER_UTILS_H

#include <glm/glm.hpp>
#include "glm/ext/matrix_clip_space.hpp"

namespace render_utils
{
    // static glm::mat4 perspectiveToOrtho(const glm::mat4& perspective)
    // {
    //     // Extract perspective parameters
    //     float near = perspective[3][2] / (perspective[2][2] - 1.0f);
    //     float far = perspective[3][2] / (perspective[2][2] + 1.0f);
    //
    //     // Calculate dimensions at near plane
    //     float left = near * (perspective[2][0] - 1.0f) / perspective[0][0];
    //     float right = near * (perspective[2][0] + 1.0f) / perspective[0][0];
    //     float bottom = near * (perspective[2][1] - 1.0f) / perspective[1][1];
    //     float top = near * (perspective[2][1] + 1.0f) / perspective[1][1];
    //
    //     // Create orthographic matrix
    //     return glm::orthoZO(left, right, bottom, top, near, far);
    // }

    static glm::mat4 perspectiveToOrtho(const glm::mat4& perspective)
    {
        // Extract the frustum planes from the perspective matrix
        float nearPlane = perspective[3][2] / (perspective[2][2] - 1.0f);
        float farPlane = perspective[3][2] / (perspective[2][2] + 1.0f);
        float left = nearPlane * (perspective[2][0] - perspective[0][3]) / perspective[0][0];
        float right = nearPlane * (perspective[2][0] + perspective[0][3]) / perspective[0][0];
        float bottom = nearPlane * (perspective[2][1] - perspective[1][3]) / perspective[1][1];
        float top = nearPlane * (perspective[2][1] + perspective[1][3]) / perspective[1][1];

        // Build the orthographic projection matrix
        return glm::ortho(left, right, bottom, top, nearPlane, farPlane);
    }

    inline glm::mat4 createOrthographicMatrix(const float width, const float height, const float nearPlane, const float farPlane)
    {
        const float right = width / 2.0f;
        const float left = -right;
        const float top = height / 2.0f;
        const float bottom = -top;
        const float near = nearPlane;
        const float far = farPlane;

        auto ortho = glm::mat4(1.0f);

        // Scale
        ortho[0][0] = 2.0f / (right - left);
        ortho[1][1] = 2.0f / (top - bottom);
        ortho[2][2] = 2.0f / (near - far);

        // Translation
        ortho[3][0] = -(right + left) / (right - left);
        ortho[3][1] = -(top + bottom) / (top - bottom);
        ortho[3][2] = (near + far) / (near - far);

        return ortho;
    }
}

#endif
