//
// Created by William on 2024-08-24.

#ifndef FREE_CAMERA_H
#define FREE_CAMERA_H
#include "src/core/camera/camera.h"


/**
 * Free camera is a no-clip like camera with free movement.
 */
class FreeCamera final : public Camera {
public:
    explicit FreeCamera(float fov = 75.0f, float aspectRatio = 1700.0f / 900.0f, float nearPlane = 100.0f, float farPlane = 0.0001f);

    ~FreeCamera() override = default;

    void update() override;

    float getZVelocity() const override;
private:
    float speed{1.0f};
};

#endif //FREE_CAMERA_H