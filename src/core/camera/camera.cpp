//
// Created by William on 2024-08-24.
//

#include "camera.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

Camera::Camera(const float fov, const float aspect, const float nearPlane, const float farPlane)
{
    // depth buffer is reversed in this engine.
    updateProjMatrix(fov, aspect, nearPlane, farPlane);
    updateViewMatrix();
    updateViewProjMatrix();
}

glm::mat4 Camera::getRotationMatrixWS() const
{
    return glm::mat4_cast(transform.getRotation());
}

glm::vec3 Camera::getViewDirectionWS() const
{
    return transform.getRotation() * glm::vec3(0.0f, 0.0f, -1.0f);
}

void Camera::updateProjMatrix(float fov, float aspect, float nearPlane, float farPlane)
{
    // depth buffer is reversed in this engine.
    cachedProjMatrix = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
    if (flipProjectionY) { cachedProjMatrix[1][1] *= -1; }
    cachedFov = fov;
    cachedAspect = aspect;
    cachedNear = nearPlane;
    cachedFar = farPlane;
    updateViewProjMatrix();
}

void Camera::updateViewMatrix()
{
    const glm::vec3 position = transform.getPosition();
    const glm::vec3 forward = getViewDirectionWS();
    constexpr auto up = glm::vec3(0.0f, 1.0f, 0.0f);
    cachedViewMatrix = glm::lookAt(position, position + forward, up);
    updateViewProjMatrix();
}


void Camera::updateViewProjMatrix()
{
    cachedViewProjMatrix = cachedProjMatrix * cachedViewMatrix;
}
