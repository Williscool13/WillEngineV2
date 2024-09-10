//
// Created by William on 2024-08-24.
//

#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "glm/glm.hpp"
#include "glm/ext/matrix_transform.hpp"

typedef glm::vec3 Position;
typedef glm::vec3 Rotation;
typedef glm::vec3 Scale;

class Transform
{
private:
    Position position;
    Rotation rotation; // Euler angles in radians (pitch, yaw, roll)
    Scale scale;
    mutable glm::mat4 cachedTRSMatrix;
    mutable bool isDirty;

public:
    Transform() : position(0.0f), rotation(0.0f), scale(1.0f), cachedTRSMatrix(1.0f), isDirty(true) {}

    Transform(const glm::vec3 position, const glm::vec3 rotation, const glm::vec3 scale)
        : position(position), rotation(rotation), scale(scale), cachedTRSMatrix(1.0f), isDirty(true) {}


    Position getPosition() const { return position; }
    Rotation getRotation() const { return rotation; }
    Scale getScale() const { return scale; }

    void setPosition(const Position& position)
    {
        this->position = position;
        isDirty = true;
    }

    void setRotation(const Rotation& rotation)
    {
        this->rotation = rotation;
        isDirty = true;
    }

    void setScale(const Scale& scale)
    {
        this->scale = scale;
        isDirty = true;
    }

    void translate(const Position& offset)
    {
        position += offset;
        isDirty = true;
    }

    void rotate(const Rotation& angles)
    {
        rotation += angles;
        isDirty = true;
    }

    glm::mat4 getTRSMatrix() const
    {
        if (isDirty) {
            const glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);

            glm::mat4 rotationMatrix{1.0f};
            rotationMatrix = glm::rotate(rotationMatrix, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f)); // Pitch
            rotationMatrix = glm::rotate(rotationMatrix, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f)); // Yaw
            rotationMatrix = glm::rotate(rotationMatrix, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f)); // Roll

            const glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);

            cachedTRSMatrix = translationMatrix * rotationMatrix * scaleMatrix;
            isDirty = false;
        }
        return cachedTRSMatrix;
    }

    void reset()
    {
        position = glm::vec3(0.0f);
        rotation = glm::vec3(0.0f);
        scale = glm::vec3(1.0f);
        isDirty = true;
    }
};


#endif //TRANSFORM_H
