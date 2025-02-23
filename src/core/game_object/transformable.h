//
// Created by William on 2025-01-27.
//

#ifndef TRANSFORMABLE_H
#define TRANSFORMABLE_H

#include <glm/glm.hpp>

#include "src/core/transform.h"

namespace will_engine
{
class ITransformable
{
public:
    virtual ~ITransformable() = default;

    virtual glm::mat4 getModelMatrix() = 0;

    [[nodiscard]] virtual const Transform& getLocalTransform() const = 0;

    [[nodiscard]] virtual glm::vec3 getLocalPosition() const = 0;

    [[nodiscard]] virtual glm::quat getLocalRotation() const = 0;

    [[nodiscard]] virtual glm::vec3 getLocalScale() const = 0;

    [[nodiscard]] virtual const Transform& getGlobalTransform() = 0;

    virtual glm::vec3 getGlobalPosition() = 0;

    virtual glm::quat getGlobalRotation() = 0;

    virtual glm::vec3 getGlobalScale() = 0;

    virtual void setLocalPosition(glm::vec3 localPosition) = 0;

    virtual void setLocalRotation(glm::quat localRotation) = 0;

    virtual void setLocalScale(glm::vec3 localScale) = 0;

    virtual void setLocalScale(float localScale) = 0;

    virtual void setLocalTransform(const Transform& newLocalTransform) = 0;

    virtual void setGlobalPosition(glm::vec3 globalPosition) = 0;

    virtual void setGlobalRotation(glm::quat globalRotation) = 0;

    virtual void setGlobalScale(glm::vec3 newScale) = 0;

    virtual void setGlobalScale(float globalScale) = 0;

    virtual void setGlobalTransform(const Transform& newGlobalTransform) = 0;

    virtual void setGlobalTransformFromPhysics(const glm::vec3& position, const glm::quat& rotation) = 0;

    virtual void translate(glm::vec3 translation) = 0;

    virtual void rotate(glm::quat rotation) = 0;

    virtual void rotateAxis(float angle, const glm::vec3& axis) = 0;
};
}


#endif //TRANSFORMABLE_H
