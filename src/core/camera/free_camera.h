//
// Created by William on 2024-08-24.

#ifndef FREE_CAMERA_H
#define FREE_CAMERA_H
#include "camera.h"


/**
 * Free camera is a no-clip like camera with free movement.
 */
class FreeCamera : public Camera {
public:
    explicit FreeCamera(float fov = 75.0f, float aspectRatio = 1920.0f / 1080.0f, float farPlane = 10000.0f, float nearPlane = 0.1f);
    virtual ~FreeCamera() = default;

    void update() override;
};

#endif //FREE_CAMERA_H