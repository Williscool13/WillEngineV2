//
// Created by William on 2024-08-24.
//

#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "glm/glm.hpp"
#include "glm/detail/type_quat.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtx/quaternion.hpp"


class Transform
{
private:
    glm::vec3 translation;
    glm::quat rotation;
    glm::vec3 scale;

    mutable glm::mat4 cachedRotationMatrix{1.0f};
    mutable bool rotationDirty{true};

    mutable glm::mat4 cachedTRSMatrix{1.0f};
    mutable bool trsDirty{true};

public:
    Transform() : translation(0.0f), rotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)), scale(1.0f) {}
    Transform(const glm::vec3& translation, const glm::quat& rotation, const glm::vec3& scale) : translation(translation), rotation(rotation), scale(scale) {}

    [[nodiscard]] glm::vec3 getTranslation() const { return translation; }
    [[nodiscard]] glm::vec3 getEulerAngles() const
    {
        return glm::eulerAngles(rotation);
    }
    [[nodiscard]] glm::quat getRotation() const { return rotation; }
    [[nodiscard]] glm::vec3 getScale() const { return scale; }

    [[nodiscard]] glm::mat4 getTranslationMatrix() const
    {
        return glm::translate(glm::mat4(1.0f), translation);
    }
    [[nodiscard]] glm::mat4 getRotationMatrix() const
    {
        if (rotationDirty) {
            cachedRotationMatrix = glm::toMat4(rotation);
            rotationDirty = false;
        }
        return cachedRotationMatrix;
    }
    [[nodiscard]] glm::mat4 getScaleMatrix() const
    {
        return glm::scale(glm::mat4(1.0f), scale);
    }

    void setTranslation(const glm::vec3& translation)
    {
        this->translation = translation;
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
        setTranslation(transform.getTranslation());
        setRotation(transform.getRotation());
        setScale(transform.getScale());
    }
    void setTransform(const glm::vec3& translation, const glm::quat& rotation, const glm::vec3& scale)
    {
        setTranslation(translation);
        setRotation(rotation);
        setScale(scale);
    }
    void setDirty() const
    {
        trsDirty = true;
        rotationDirty = true;
    }

    void translate(const glm::vec3& offset)
    {
        translation += offset;
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
        glm::quat rotationDelta = glm::angleAxis(angle, glm::normalize(axis));
        rotate(rotationDelta);
    }

    glm::mat4 getTRSMatrix() const
    {
        if (trsDirty) {
            cachedTRSMatrix = getTranslationMatrix() * getRotationMatrix() * getScaleMatrix();
            trsDirty = false;
        }
        return cachedTRSMatrix;
    }

    void reset()
    {
        translation = glm::vec3(0.0f);
        rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        scale = glm::vec3(1.0f);
        trsDirty = true;
    }
};


#endif //TRANSFORM_H
