//
// Created by William on 2024-08-24.
//

#include "camera.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

will_engine::Camera::Camera(const float fov, const float aspect, const float nearPlane, const float farPlane)
{
    updateProjMatrix(fov, aspect, nearPlane, farPlane);
    updateViewMatrix();
    updateViewProjMatrix();
}

glm::mat4 will_engine::Camera::getRotationMatrixWS() const
{
    return glm::mat4_cast(transform.getRotation());
}

glm::vec3 will_engine::Camera::getForwardWS() const
{
    return transform.getRotation() * glm::vec3(0.0f, 0.0f, -1.0f);
}

glm::vec3 will_engine::Camera::getUpWS() const
{
    return transform.getRotation() * glm::vec3(0.0f, 1.0f, 0.0f);
}

glm::vec3 will_engine::Camera::getRightWS() const
{
    return transform.getRotation() * glm::vec3(1.0f, 0.0f, 0.0f);
}

void will_engine::Camera::updateProjMatrix(float fov, float aspect, float nearPlane, float farPlane)
{
    cachedFov = fov;
    cachedAspect = aspect;
    cachedNear = nearPlane;
    cachedFar = farPlane;

    // depth buffer is reversed in this engine.
    cachedProjMatrix = glm::perspective(fov, aspect, nearPlane, farPlane);
    updateViewProjMatrix();
}

void will_engine::Camera::updateViewMatrix()
{
    const glm::vec3 position = transform.getPosition();
    const glm::vec3 forward = getForwardWS();
    constexpr auto up = glm::vec3(0.0f, 1.0f, 0.0f);
    cachedViewMatrix = glm::lookAt(position, position + forward, up);
    updateViewProjMatrix();
}


void will_engine::Camera::updateViewProjMatrix()
{
    cachedViewProjMatrix = cachedProjMatrix * cachedViewMatrix;
}

void will_engine::Camera::setProjectionProperties(const float fov, const float aspect, const float nearPlane, const float farPlane)
{
    updateProjMatrix(fov, aspect, nearPlane, farPlane);
}

void will_engine::Camera::setCameraTransform(const glm::vec3 position, const glm::quat rotation)
{
    transform.setPosition(position);
    transform.setRotation(rotation);
    updateViewMatrix();
}
