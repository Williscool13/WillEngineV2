//
// Created by William on 2024-08-24.
//

#include "camera.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

Camera::Camera(float fov, float aspect, float farPlane, float nearPlane)
{
    // depth buffer is reversed in this engine.
    updateProjMatrix(fov, aspect, farPlane, nearPlane);
    updateViewMatrix();
    updateViewProjMatrix();
}

void Camera::update()
{}

glm::mat4 Camera::getRotationMatrixWS() const
{
    glm::mat4 pitchMatrix = glm::rotate(glm::mat4(1.0f), pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 yawMatrix = glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0.0f, -1.0f, 0.0f));

    return yawMatrix * pitchMatrix;
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
    cachedFov = fov;
    cachedAspect = aspect;
    cachedFar = farPlane;
    cachedNear = nearPlane;
    updateViewProjMatrix();
}

void Camera::updateViewMatrix()
{
    glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), position);
    glm::mat4 cameraRotation = getRotationMatrixWS();
    cachedViewMatrix = glm::inverse(cameraTranslation * cameraRotation);
    updateViewProjMatrix();
}


void Camera::updateViewProjMatrix()
{
    cachedViewProjMatrix = cachedProjMatrix * cachedViewMatrix;
}
