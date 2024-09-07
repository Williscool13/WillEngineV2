//
// Created by William on 2024-08-24.
//

#include "camera.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

Camera::Camera(const float fov, const float aspect, const float farPlane, const float nearPlane, const bool flipY) : flipY(flipY)
{
    // depth buffer is reversed in this engine.
    updateProjMatrix(fov, aspect, farPlane, nearPlane);
    updateViewMatrix();
    updateViewProjMatrix();
}

glm::mat4 Camera::getRotationMatrixWS() const
{
    return glm::mat4_cast(glm::quat(transform.getRotation()));
}

glm::vec3 Camera::getViewDirectionWS() const
{
    glm::mat4 rotationMatrix = getRotationMatrixWS();
    return -glm::vec3(rotationMatrix[2]);
}

void Camera::updateProjMatrix(float fov, float aspect, float farPlane, float nearPlane)
{
    // depth buffer is reversed in this engine.
    cachedProjMatrix = glm::perspective(glm::radians(fov), aspect, farPlane, nearPlane);
    //cachedProjMatrix[1][1] *= -1;
    cachedFov = fov;
    cachedAspect = aspect;
    cachedFar = farPlane;
    cachedNear = nearPlane;
    updateViewProjMatrix();
}

void Camera::updateViewMatrix()
{
    glm::vec3 position = transform.getPosition();
    glm::vec3 forward = getViewDirectionWS();
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    cachedViewMatrix = glm::lookAt(position, position + forward, up);
    //cachedViewMatrix = glm::inverse(transform.getWorldMatrix());
    updateViewProjMatrix();
}


void Camera::updateViewProjMatrix()
{
    cachedViewProjMatrix = cachedProjMatrix * cachedViewMatrix;
}
