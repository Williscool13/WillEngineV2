//
// Created by William on 2024-08-24.
//

#include "free_camera.h"

#include "../input.h"
#include "../../util/time_utils.h"
#include "glm/common.hpp"
#include "glm/gtc/constants.hpp"

FreeCamera::FreeCamera(const float fov, const float aspect, const float farPlane, const float nearPlane) : Camera(fov, aspect, farPlane, nearPlane)
{}

void FreeCamera::update()
{
    const Input& input = Input::Get();
    if (!input.isInFocus()) {
        return;
    }

    glm::vec3 velocity{0.f};
    float verticalMove{0.f};

    if (input.isKeyDown(SDLK_d)) {
        velocity.x += 1.0f;
    }
    if (input.isKeyDown(SDLK_a)) {
        velocity.x -= 1.0f;
    }
    if (input.isKeyDown(SDLK_q)) {
        verticalMove -= 1.0f;
    }
    if (input.isKeyDown(SDLK_e)) {
        verticalMove += 1.0f;
    }

    // I guess vulkan is negative Z forward?!
    if (input.isKeyDown(SDLK_w)) {
        velocity.z -= 1.0f;
    }
    if (input.isKeyDown(SDLK_s)) {
        velocity.z += 1.0f;
    }

    const float deltaTime = TimeUtils::Get().getDeltaTime();

    constexpr float minSpeed = 0.1f;
    constexpr float maxSpeed = 10.0f;
    constexpr float sensitivity = 0.1f;

    float logSpeed = glm::log(speed);
    logSpeed += sensitivity * input.getMouseWheelDelta();
    speed = glm::exp(logSpeed);
    speed = glm::clamp(speed, minSpeed, maxSpeed);
    /*speed *= glm::pow(1.15f, input.getMouseWheelDelta());
    speed = glm::clamp(speed, 0.1f, 10.0f);*/

    velocity *= deltaTime * speed;
    verticalMove *= deltaTime * speed;

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
    transform.translate(glm::vec3(rotationMatrix * glm::vec4(velocity, 0.f)));
    transform.translate(glm::vec3(0.f, verticalMove, 0.f));

    updateViewMatrix();
}
