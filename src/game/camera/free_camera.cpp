//
// Created by William on 2024-08-24.
//

#include "free_camera.h"

#include "glm/common.hpp"
#include "src/core/input.h"
#include "src/util/time_utils.h"

FreeCamera::FreeCamera(const float fov, const float aspect, const float nearPlane, const float farPlane) : Camera(fov, aspect, nearPlane, farPlane)
{}

void FreeCamera::update(const float deltaTime)
{
    const Input& input = Input::Get();
    if (!input.isInFocus()) {
        return;
    }

    glm::vec3 velocity{0.f};

    if (input.isKeyDown(SDLK_d)) {
        velocity.x += 1.0f;
    }
    if (input.isKeyDown(SDLK_a)) {
        velocity.x -= 1.0f;
    }
    if (input.isKeyDown(SDLK_LCTRL)) {
        velocity.y -= 1.0f;
    }
    if (input.isKeyDown(SDLK_SPACE)) {
        velocity.y += 1.0f;
    }
    // I guess vulkan is negative Z forward?!
    if (input.isKeyDown(SDLK_w)) {
        velocity.z -= 1.0f;
    }
    if (input.isKeyDown(SDLK_s)) {
        velocity.z += 1.0f;
    }

    if (input.isKeyPressed(SDLK_PERIOD)) {
        speed += 1;
    }
    if (input.isKeyPressed(SDLK_COMMA)) {
        speed -= 1;
    }
    speed = glm::clamp(speed, -2.0f, 3.0f);

    float scale = speed;
    if (scale <= 0) {
        scale -= 1;
    }
    const auto currentSpeed = static_cast<float>(glm::pow(10, scale));

    velocity *= deltaTime * currentSpeed;

    const float yaw = glm::radians(-input.getMouseXDelta() * deltaTime * 10.0f);
    const float pitch = glm::radians(-input.getMouseYDelta() * deltaTime * 10.0f);

    const glm::quat currentRotation = transform.getRotation();
    const glm::vec3 forward = currentRotation * glm::vec3(0.0f, 0.0f, -1.0f);
    const float currentPitch = std::asin(forward.y);

    const float newPitch = glm::clamp(currentPitch + pitch, glm::radians(-89.9f), glm::radians(89.9f));
    const float pitchDelta = newPitch - currentPitch;

    const glm::quat yawQuat = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::quat pitchQuat = glm::angleAxis(pitchDelta, glm::vec3(1.0f, 0.0f, 0.0f));

    glm::quat newRotation = yawQuat * currentRotation * pitchQuat;
    newRotation = glm::normalize(newRotation);

    transform.setRotation(newRotation);

    const glm::mat4 rotationMatrix = getRotationMatrixWS();
    const auto finalVelocity = glm::vec3(rotationMatrix * glm::vec4(velocity, 0.f));
    transform.translate(finalVelocity);

    updateViewMatrix();
}