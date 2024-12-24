//
// Created by William on 2024-12-24.
//

#ifndef ORBIT_CAMERA_H
#define ORBIT_CAMERA_H
#include "src/core/camera/camera.h"


class GameObject;

class OrbitCamera final : public Camera
{
public:
    explicit OrbitCamera(float fov = 75.0f, float aspect = 1700.0f / 900.0f, float nearPlane = 100.0f, float farPlane = 0.0001f);

    ~OrbitCamera() override = default;

    void update(float deltaTime) override;

public:
    void setOrbitTarget(GameObject* gameObject) { orbitTarget = gameObject; }

private:
    GameObject* orbitTarget{nullptr};

    float rotationSpeed{1.0f};
    glm::vec3 armOffset{0, 0, -4.0f};
};


#endif //ORBIT_CAMERA_H
