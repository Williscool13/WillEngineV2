//
// Created by William on 2024-08-24.

#ifndef CAMERA_H
#define CAMERA_H
#include <glm/glm.hpp>


/**
 * The base class that defines a camera.
 * \n Subclasses should individually define how pitch, yaw, and position are updated.
 * \n Note that in this game engine, the depth buffer is reversed for a better distributed depth curve.
 */
class Camera
{
public:
    glm::mat4 getViewMatrix() const
    {
        return cachedViewMatrix;
    }

    glm::mat4 getProjMatrix() const
    {
        return cachedProjMatrix;
    }

    glm::mat4 getViewProjMatrix() const
    {
        return cachedViewProjMatrix;
    }

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


    virtual void updateProjMatrix(float fov, float aspect, float farPlane, float nearPlane);

    virtual void updateViewMatrix();

    void updateViewProjMatrix();

protected:
    Camera() = default;

    Camera(float fov, float aspect, float farPlane, float nearPlane);

    ~Camera() = default;

    virtual void update();

protected:
    float pitch{0.f};
    float yaw{0.f};
    glm::vec3 position{0., 0., 0.};

    // projection
    float cachedFov{75.0f};
    float cachedAspect{1920.0f / 1080.0f};
    float cachedFar{10000.0f};
    float cachedNear{0.1f};

    glm::mat4 cachedViewMatrix{};
    glm::mat4 cachedProjMatrix{};
    glm::mat4 cachedViewProjMatrix{};
};


#endif //CAMERA_H