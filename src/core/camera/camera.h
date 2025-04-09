//
// Created by William on 2024-08-24.

#ifndef CAMERA_H
#define CAMERA_H

#include "camera_types.h"
#include "src/core/transform.h"
#include "src/renderer/renderer_constants.h"

namespace will_engine
{
/**
 * The base class that defines a camera.
 * \n Subclasses should individually define how pitch, yaw, and position are updated.
 * \n Note that in this game engine, the depth buffer is reversed for a better distributed depth curve.
 */
class Camera
{
public:
    explicit Camera(float fov = 1.308996939f, float aspect = RENDER_EXTENT_WIDTH / RENDER_EXTENT_HEIGHT, float nearPlane = 1000.0f,
                    float farPlane = 0.1f);

    virtual ~Camera() = default;

public:
    Transform getTransform() const { return transform; }
    glm::vec4 getPosition() const { return {transform.getPosition(), 1.0f}; }
    glm::mat4 getViewMatrix() const { return cachedViewMatrix; }
    glm::mat4 getProjMatrix() const { return cachedProjMatrix; }
    glm::mat4 getViewProjMatrix() const { return cachedViewProjMatrix; }
    float getNearPlane() const { return cachedNear; }
    float getFarPlane() const { return cachedFar; }
    float getAspectRatio() const { return cachedAspect; }
    float getFov() const { return cachedFov; }
    /**
     * Generates the rotation matrix of the camera in world space using the \code pitch\endcode and \code yaw\endcode  properties
     * @return
     */
    glm::mat4 getRotationMatrixWS() const;

    /**
     * Gets the viewing direction of the camera in world space
     * @return
     */
    glm::vec3 getForwardWS() const;

    glm::vec3 getUpWS() const;

    glm::vec3 getRightWS() const;

    virtual void update(float deltaTime) = 0;


    CameraProperties getCameraProperties() const
    {
        return {
            cachedFov, cachedAspect, cachedNear, cachedFar, cachedViewMatrix, cachedProjMatrix, transform.getPosition(), getForwardWS(), getRightWS(),
            getUpWS()
        };
    }

    void setProjectionProperties(float fov = 1.308996939f, float aspect = RENDER_EXTENT_WIDTH / RENDER_EXTENT_HEIGHT, float nearPlane = 1000.0f,
                                 float farPlane = 0.1f);

    void setCameraTransform(glm::vec3 position, glm::quat rotation);

protected:
    /**
     * Updates the projection matrix. FOV is in degrees and converted to radians in this function
     * @param fov
     * @param aspect
     * @param nearPlane
     * @param farPlane
     */
    virtual void updateProjMatrix(float fov, float aspect, float nearPlane, float farPlane);

    /**
     * Must be called after all updates to camera position to update the matrices
     */
    virtual void updateViewMatrix();

    void updateViewProjMatrix();

protected:
    Transform transform;

    float cachedFov{1.308996939}; // rad(75)
    float cachedAspect{1920.0f / 1080.0f};
    float cachedNear{10000.0f};
    float cachedFar{0.1f};

    glm::mat4 cachedViewMatrix{};
    glm::mat4 cachedProjMatrix{};
    glm::mat4 cachedViewProjMatrix{};
};
}


#endif //CAMERA_H
