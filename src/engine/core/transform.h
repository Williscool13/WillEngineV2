//
// Created by William on 2024-08-24.
//

#ifndef TRANSFORM_H
#define TRANSFORM_H

#include <glm/glm.hpp>
#include <glm/detail/type_quat.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace will_engine
{
class Transform
{
private:
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

public:
    Transform() = default;

    Transform(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale) : position(position), rotation(rotation), scale(scale) {}

    glm::vec3 getPosition() const { return position; }
    glm::quat getRotation() const { return rotation; }
    glm::vec3 getScale() const { return scale; }

    void setPosition(const glm::vec3 position) { this->position = position; }
    void setRotation(const glm::quat rotation) { this->rotation = rotation; }
    void setScale(const glm::vec3 scale) { this->scale = scale; }

    void setTransform(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale)
    {
        this->position = position;
        this->rotation = rotation;
        this->scale = scale;
    }

    void reset()
    {
        position = glm::vec3(0.0f);
        rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        scale = glm::vec3(1.0f);
    }

    void translate(const glm::vec3& translation)
    {
        position += translation;
    }

    void rotate(const glm::vec3& angles)
    {
        const auto rotationDelta = glm::quat(angles);
        this->rotation = this->rotation * rotationDelta;
    }

    void rotate(const glm::quat& angles)
    {
        this->rotation = this->rotation * angles;
    }

    void rotateDegree(const glm::vec3& angles)
    {
        this->rotation = this->rotation * glm::radians(angles);
    }

    void rotateAxis(const float angle, const glm::vec3& axis)
    {
        const glm::quat rotationDelta = glm::angleAxis(angle, glm::normalize(axis));
        rotate(rotationDelta);
    }

    Transform transformBy(const Transform& parent) const
    {
        Transform result;
        result.scale = parent.scale * this->scale;
        result.rotation = parent.rotation * this->rotation;
        result.position = parent.position + parent.rotation * (parent.scale * this->position);
        return result;
    }

    void applyParentTransform(const Transform& parent)
    {
        // Apply parent transform to this transform
        position = parent.position + parent.rotation * (parent.scale * position);
        rotation = parent.rotation * rotation;
        scale = parent.scale * scale;
    }

    glm::mat4 toModelMatrix() const
    {
        return glm::translate(glm::mat4(1.0f), position) *
               mat4_cast(rotation) *
               glm::scale(glm::mat4(1.0f), scale);
    }

public:
    static const Transform Identity;
};
}


#endif //TRANSFORM_H
