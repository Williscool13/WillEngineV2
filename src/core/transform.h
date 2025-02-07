//
// Created by William on 2024-08-24.
//

#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "extern/glm/glm/glm.hpp"
#include "extern/glm/glm/detail/type_quat.hpp"
#include "extern/glm/glm/ext/matrix_transform.hpp"
#include "extern/glm/glm/gtx/quaternion.hpp"

namespace will_engine
{
class Transform
{
private:
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    mutable glm::mat4 cachedRotationMatrix{1.0f};
    mutable bool rotationDirty{true};

    mutable glm::mat4 cachedTRSMatrix{1.0f};
    mutable bool trsDirty{true};

public:
    Transform() = default;

    Transform(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale) : position(position), rotation(rotation), scale(scale) {}

    [[nodiscard]] glm::vec3 getPosition() const { return position; }
    [[nodiscard]] glm::vec3 getEulerAngles() const { return glm::eulerAngles(rotation); }
    [[nodiscard]] glm::quat getRotation() const { return rotation; }
    [[nodiscard]] glm::vec3 getScale() const { return scale; }
    [[nodiscard]] glm::mat4 getPositionMatrix() const { return glm::translate(glm::mat4(1.0f), position); }

    [[nodiscard]] glm::mat4 getRotationMatrix() const
    {
        if (rotationDirty) {
            cachedRotationMatrix = glm::toMat4(rotation);
            rotationDirty = false;
        }
        return cachedRotationMatrix;
    }

    [[nodiscard]] glm::mat4 getScaleMatrix() const { return glm::scale(glm::mat4(1.0f), scale); }
    glm::vec3 getForward() const { return glm::normalize(glm::rotate(rotation, glm::vec3(0.0f, 0.0f, -1.0f))); }
    glm::vec3 getRight() const { return glm::normalize(glm::rotate(rotation, glm::vec3(1.0f, 0.0f, 0.0f))); }
    glm::vec3 getUp() const { return glm::normalize(glm::rotate(rotation, glm::vec3(0.0f, 1.0f, 0.0f))); }
    glm::vec3 transformDirection(const glm::vec3& direction) const { return glm::normalize(glm::rotate(rotation, direction)); }

    void setPosition(const glm::vec3& position)
    {
        this->position = position;
        trsDirty = true;
    }

    void setRotation(const glm::quat& rotation)
    {
        this->rotation = rotation;
        rotationDirty = true;
        trsDirty = true;
    }

    void setEulerRotation(const glm::vec3& eulerAngles)
    {
        this->rotation = glm::quat(eulerAngles);
        rotationDirty = true;
        trsDirty = true;
    }

    void setScale(const glm::vec3& scale)
    {
        this->scale = scale;
        trsDirty = true;
    }

    void setScale(const float scale)
    {
        this->scale = glm::vec3(scale);
        trsDirty = true;
    }

    void setTransform(const Transform& transform)
    {
        setPosition(transform.getPosition());
        setRotation(transform.getRotation());
        setScale(transform.getScale());
        trsDirty = true;
    }

    void setTransform(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale)
    {
        setPosition(position);
        setRotation(rotation);
        setScale(scale);
        trsDirty = true;
    }

    void setDirty() const
    {
        trsDirty = true;
        rotationDirty = true;
    }

    void translate(const glm::vec3& offset)
    {
        position += offset;
        trsDirty = true;
    }

    /**
     * Rotate in radians
     * @param angles
     */
    void rotate(const glm::vec3& angles)
    {
        const auto rotationDelta = glm::quat(angles);
        this->rotation = this->rotation * rotationDelta;
        rotationDirty = true;
        trsDirty = true;
    }

    /**
     * Rotate in radians
     * @param angles
     */
    void rotate(const glm::quat& angles)
    {
        this->rotation = this->rotation * angles;
        rotationDirty = true;
        trsDirty = true;
    }

    void rotateAxis(const float angle, const glm::vec3& axis)
    {
        const glm::quat rotationDelta = glm::angleAxis(angle, glm::normalize(axis));
        rotate(rotationDelta);
    }

    glm::mat4 getTRSMatrix() const
    {
        if (trsDirty) {
            cachedTRSMatrix = getPositionMatrix() * getRotationMatrix() * getScaleMatrix();
            trsDirty = false;
        }
        return cachedTRSMatrix;
    }

    void reset()
    {
        position = glm::vec3(0.0f);
        rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        scale = glm::vec3(1.0f);
        trsDirty = true;
    }

public:
    static const Transform Identity;
};
}


#endif //TRANSFORM_H
