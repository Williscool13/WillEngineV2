//
// Created by William on 2025-01-26.
//

#ifndef RENDER_UTILS_H
#define RENDER_UTILS_H
#include <glm/glm.hpp>
#include "glm/ext/matrix_clip_space.hpp"

namespace will_engine::render_utils
{
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

inline void getPerspectiveFrustumCornersWorldSpace(const float nearPlane, const float farPlane, const float fov, const float aspect, const glm::vec3& position, const glm::vec3& viewDir,
                                                   glm::vec4 corners[8])
{
    constexpr glm::vec3 up{0.0f, 1.0f, 0.0f};

    const glm::vec3 right = glm::normalize(glm::cross(viewDir, up));
    const glm::vec3 up_corrected = glm::normalize(glm::cross(right, viewDir));

    const float near_height = glm::tan(fov * 0.5f) * nearPlane;
    const float near_width = near_height * aspect;
    const float far_height = glm::tan(fov * 0.5f) * farPlane;
    const float far_width = far_height * aspect;

    const glm::vec3 near_center = position + viewDir * nearPlane;
    corners[0] = glm::vec4(near_center - up_corrected * near_height - right * near_width, 1.0f); // bottom-left
    corners[1] = glm::vec4(near_center + up_corrected * near_height - right * near_width, 1.0f); // top-left
    corners[2] = glm::vec4(near_center + up_corrected * near_height + right * near_width, 1.0f); // top-right
    corners[3] = glm::vec4(near_center - up_corrected * near_height + right * near_width, 1.0f); // bottom-right

    // Far face corners
    const glm::vec3 far_center = position + viewDir * farPlane;
    corners[4] = glm::vec4(far_center - up_corrected * far_height - right * far_width, 1.0f); // bottom-left
    corners[5] = glm::vec4(far_center + up_corrected * far_height - right * far_width, 1.0f); // top-left
    corners[6] = glm::vec4(far_center + up_corrected * far_height + right * far_width, 1.0f); // top-right
    corners[7] = glm::vec4(far_center - up_corrected * far_height + right * far_width, 1.0f); // bottom-right
}

inline void getOrthoFrustumCornersWorldSpace(const float left, const float right, const float bottom, const float top, const float zNear, const float zFar, glm::vec4 corners[8])
{
    // Near face corners (counter-clockwise from bottom-left)
    corners[0] = glm::vec4(left, bottom, zNear, 1.0f); // bottom-left
    corners[1] = glm::vec4(left, top, zNear, 1.0f); // top-left
    corners[2] = glm::vec4(right, top, zNear, 1.0f); // top-right
    corners[3] = glm::vec4(right, bottom, zNear, 1.0f); // bottom-right

    // Far face corners (counter-clockwise from bottom-left)
    corners[4] = glm::vec4(left, bottom, zFar, 1.0f); // bottom-left
    corners[5] = glm::vec4(left, top, zFar, 1.0f); // top-left
    corners[6] = glm::vec4(right, top, zFar, 1.0f); // top-right
    corners[7] = glm::vec4(right, bottom, zFar, 1.0f); // bottom-right
}

inline void getFrustumCurnersFromCropMatrix(const glm::mat4& cropMatrix, glm::vec3 corners[8])
{
    glm::mat4 invCrop = glm::inverse(cropMatrix);

    int idx = 0;

    for (int z = 0; z < 2; z++) {
        for (int y = 0; y < 2; y++) {
            for (int x = 0; x < 2; x++) {
                // Create point in NDC space
                glm::vec4 corner = invCrop * glm::vec4(
                                       x * 2.0f - 1.0f, // -1 or 1
                                       y * 2.0f - 1.0f, // -1 or 1
                                       z * 2.0f - 1.0f, // -1 or 1
                                       1.0f
                                   );

                // Perform perspective divide
                corners[idx++] = glm::vec3(corner) / corner.w;
            }
        }
    }
}

inline std::array<glm::vec3, 8> getLightSpaceCorners(const glm::mat4& lightViewProj)
{
    // Start with canonical cube corners (-1 to 1 in NDC)
    const std::array<glm::vec4, 8> ndc_corners = {
        {
            {-1, -1, -1, 1}, {1, -1, -1, 1}, // Near plane (bottom left, bottom right)
            {1, 1, -1, 1}, {-1, 1, -1, 1}, // Near plane (top right, top left)
            {-1, -1, 1, 1}, {1, -1, 1, 1}, // Far plane (bottom left, bottom right)
            {1, 1, 1, 1}, {-1, 1, 1, 1} // Far plane (top right, top left)
        }
    };

    // Transform from NDC back to world space
    const glm::mat4 invLightViewProj = glm::inverse(lightViewProj);
    std::array<glm::vec3, 8> worldCorners;

    for (size_t i = 0; i < 8; i++) {
        glm::vec4 worldSpace = invLightViewProj * ndc_corners[i];
        worldSpace /= worldSpace.w; // Perspective divide
        worldCorners[i] = glm::vec3(worldSpace);
    }

    return worldCorners;
}
}

#endif //RENDER_UTILS_H
