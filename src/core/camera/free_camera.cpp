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

    if (input.isKeyDown(SDLK_d)) {
        velocity.x += 1.0f;
    }
    if (input.isKeyDown(SDLK_a)) {
        velocity.x -= 1.0f;
    }

    // I guess vulkan is negative Z forward?!
    if (input.isKeyDown(SDLK_w)) {
        velocity.z -= 1.0f;
    }
    if (input.isKeyDown(SDLK_s)) {
        velocity.z += 1.0f;
    }


    velocity *= TimeUtils::Get().getDeltaTime();

    Rotation rotation = transform.getRotation();
    rotation.y -= input.getMouseDeltaX() / 200.0f;
    rotation.x += input.getMouseDeltaY() / 200.0f;
    // add small epsilon to up/down to avoid gimbal lock
    constexpr float epsilon = glm::pi<float>() * 0.01f;
    rotation.x = glm::clamp(rotation.x, -glm::half_pi<float>() + epsilon, glm::half_pi<float>() - epsilon);
    transform.setRotation(rotation);

    glm::mat4 rotationMatrix = getRotationMatrixWS();
    Position newPosition = transform.getPosition() + glm::vec3(rotationMatrix * glm::vec4(velocity, 0.f));
    transform.setPosition(newPosition);

    updateViewMatrix();
}
