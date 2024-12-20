//
// Created by William on 2024-08-24.

#ifndef CAMERA_H
#define CAMERA_H
#include <glm/glm.hpp>

#include "../../util/transform.h"


/**
 * The base class that defines a camera.
 * \n Subclasses should individually define how pitch, yaw, and position are updated.
 * \n Note that in this game engine, the depth buffer is reversed for a better distributed depth curve.
 */
class Camera
{
public:
    Camera() = default;

    Camera(float fov, float aspect, float nearPlane, float farPlane, bool flipY = true);

    virtual ~Camera() = default;

public:
    Transform transform;

    glm::vec4 getPosition() const { return {transform.getPosition(), 1.0f}; }
    glm::mat4 getViewMatrix() const { return cachedViewMatrix; }
    glm::mat4 getProjMatrix() const { return cachedProjMatrix; }
    glm::mat4 getViewProjMatrix() const { return cachedViewProjMatrix; }
    float getNearPlane() const { return cachedNear; }
    float getFarPlane() const { return cachedFar; }
    /**
     * Generates the rotation matrix of the camera in world space using the \code pitch\endcode and \code yaw\endcode  properties
     * @return
     */
    glm::mat4 getRotationMatrixWS() const;

    /**
     * Gets the viewing direction of the camera in world space
     * @return
     */
    glm::vec3 getViewDirectionWS() const;

    /**
     * Updates the projection matrix. FOV is in degrees and converted to radians in this function
     * @param fov
     * @param aspect
     * @param nearPlane
     * @param farPlane
     */
    virtual void updateProjMatrix(float fov, float aspect, float nearPlane, float farPlane);

    virtual void updateViewMatrix();

    void updateViewProjMatrix();

    virtual float getZVelocity() const = 0;

    virtual void update() {}

protected:

    bool flipY{true};

    // projection
    float cachedFov{75.0f};
    float cachedAspect{1920.0f / 1080.0f};
    float cachedNear{10000.0f};
    float cachedFar{0.1f};

    glm::mat4 cachedViewMatrix{};
    glm::mat4 cachedProjMatrix{};
    glm::mat4 cachedViewProjMatrix{};

    glm::vec3 velocity{};
};


#endif //CAMERA_H
