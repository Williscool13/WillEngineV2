//
// Created by William on 2024-08-24.
//

#include "free_camera.h"

#include "../input.h"
#include "../../util/time_utils.h"
#include "glm/common.hpp"
#include "glm/gtc/constants.hpp"

FreeCamera::FreeCamera(float fov, float apsect, float farPlane, float nearPlane) : Camera(fov, apsect, farPlane, nearPlane)
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

    if (input.isKeyDown(SDLK_w)) {
        velocity.x += 1.0f;
    }
    if (input.isKeyDown(SDLK_s)) {
        velocity.x -= 1.0f;
    }


    velocity *= TimeUtils::Get().getDeltaTime();

    yaw += input.getMouseDeltaX() / 200.0f;
    pitch -= input.getMouseDeltaY() / 200.0f;
    pitch = glm::clamp(pitch, -glm::half_pi<float>(), glm::half_pi<float>());

    glm::mat4 cameraRotation = getRotationMatrixWS();
    position += glm::vec3(cameraRotation * glm::vec4(velocity * 15.0f, 0.f));

    updateViewMatrix();
}
