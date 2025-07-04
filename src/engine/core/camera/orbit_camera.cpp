//
// Created by William on 2024-12-24.
//

#include "orbit_camera.h"

#include <glm/glm.hpp>

#include "engine/core/input.h"


will_engine::OrbitCamera::OrbitCamera(const float fov, const float aspect, const float nearPlane, const float farPlane) : Camera(fov, aspect, nearPlane, farPlane)
{}

void will_engine::OrbitCamera::update(const float deltaTime)
{
    if (!orbitTarget) { return; }

    const input::Input& input = input::Input::get();
    if (input.isCursorActive()) {
        if (input.isKeyDown(input::Key::E)) {
            armOffset.z -= 2 * deltaTime;
        }
        if (input.isKeyDown(input::Key::Q)) {
            armOffset.z += 2 * deltaTime;
        }

        if (input.isKeyPressed(input::Key::PERIOD)) {
            rotationSpeed += 0.2;
        }
        if (input.isKeyPressed(input::Key::COMMA)) {
            rotationSpeed -= 0.2f;
        }
        rotationSpeed = glm::clamp(rotationSpeed, -2.0f, 3.0f);
        float scale = rotationSpeed;
        if (scale <= 0) {
            scale -= 1;
        }

        const auto currentRotationSpeed = static_cast<float>(glm::pow(10, scale));
        const float rotationVelocity{deltaTime * currentRotationSpeed * 20.0f};

        const float yaw = glm::radians(-input.getMouseXDelta() * deltaTime * rotationVelocity);
        const float pitch = glm::radians(-input.getMouseYDelta() * deltaTime * rotationVelocity);


        const glm::quat currentRotation = transform.getRotation();
        const glm::vec3 forward = currentRotation * glm::vec3(0.0f, 0.0f, -1.0f);
        const float currentPitch = std::asin(forward.y);
        const float newPitch = glm::clamp(currentPitch + pitch, glm::radians(-89.9f), glm::radians(89.9f));
        const float pitchDelta = newPitch - currentPitch;

        const glm::quat yawQuat = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::quat pitchQuat = glm::angleAxis(pitchDelta, glm::vec3(1.0f, 0.0f, 0.0f));

        const glm::quat newRotation = glm::normalize(yawQuat * currentRotation * pitchQuat);

        transform.setRotation(newRotation);
    }

    const glm::vec3 targetPos = glm::mix(lastTargetPos,orbitTarget->getPosition(),deltaTime * followSpeed);
    lastTargetPos = targetPos;
    const glm::vec3 newPosition = targetPos - transform.getRotation() * armOffset;
    transform.setPosition(newPosition);

    updateViewMatrix();
}

void will_engine::OrbitCamera::setOrbitTarget(ITransformable* gameObject)
{
    orbitTarget = gameObject;
    const glm::vec3 newPosition = orbitTarget->getPosition() - transform.getRotation() * armOffset;
    transform.setPosition(newPosition);
}
