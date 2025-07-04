//
// Created by William on 2024-08-24.

#ifndef FREE_CAMERA_H
#define FREE_CAMERA_H

#include "engine/core/camera/camera.h"
#include "engine/renderer/renderer_constants.h"


namespace will_engine
{
/**
 * Free camera is a no-clip like camera with free movement.
 */
class FreeCamera final : public Camera {
public:
    explicit FreeCamera(float fov = 1.308996939f, float aspect = DEFAULT_RENDER_EXTENT_2D.width / DEFAULT_RENDER_EXTENT_2D.height, float nearPlane = 1000.0f,
                        float farPlane = 0.1f);

    ~FreeCamera() override = default;

    void update(float deltaTime) override;
private:
    float speed{1.0f};
};
}

#endif //FREE_CAMERA_H