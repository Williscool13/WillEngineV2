//
// Created by William on 2025-01-06.
//

#ifndef CAMERA_TYPES_H
#define CAMERA_TYPES_H

#include <glm/glm.hpp>

struct CameraProperties
{
    float fov{};
    float aspect{};
    float nearPlane{};
    float farPlane{};
    glm::mat4 viewMatrix{};
    glm::mat4 projectionMatrix{};
    glm::vec3 position{};
    glm::vec3 forward{};
};

#endif //CAMERA_TYPES_H
