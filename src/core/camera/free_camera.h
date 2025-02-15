//
// Created by William on 2024-08-24.

#ifndef FREE_CAMERA_H
#define FREE_CAMERA_H

#include "src/core/camera/camera.h"
#include "src/renderer/renderer_constants.h"


namespace will_engine
{
/**
 * Free camera is a no-clip like camera with free movement.
 */
class FreeCamera final : public Camera {
public:
    FreeCamera() = default;

    FreeCamera(float fov, float aspect, float nearPlane, float farPlane);

    ~FreeCamera() override = default;

    void update(float deltaTime) override;
private:
    float speed{1.0f};
};
}

#endif //FREE_CAMERA_H