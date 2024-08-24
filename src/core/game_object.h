//
// Created by William on 2024-08-24.
//

#ifndef GAME_OBJECT_H
#define GAME_OBJECT_H
#include "../util/transform.h"


class GameObject
{
public:
    GameObject();

    explicit GameObject(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale);

    ~GameObject();

    glm::mat4 getWorldMatrix();

public: // Transform
    void translate(glm::vec3 translation)
    {
        transform.setPosition(transform.getPosition() + translation);
        setTransformDirty();
    }

    void rotate(glm::vec3 rotation)
    {
        glm::vec3 currentRotation = transform.getRotation();
        transform.setRotation(currentRotation + rotation);
        setTransformDirty();
    }

    void scale(glm::vec3 scale)
    {
        transform.setScale(transform.getScale() * scale);
        setTransformDirty();
    }

    void setPosition(const glm::vec3& position)
    {
        transform.setPosition(position);
        setTransformDirty();
    }

    void setRotation(const glm::vec3& rotation)
    {
        transform.setRotation(rotation);
        setTransformDirty();
    }

    void setScale(const glm::vec3& scale)
    {
        transform.setScale(scale);
        setTransformDirty();
    }

    void setTransformDirty()
    {
        isTransformDirty = true;
        for (auto& child: children) {
            child->setTransformDirty();
        }
    }

public: // Hierarchy
    void addChild(GameObject* child);

    void removeChild(GameObject* child);

    void unparentChild(GameObject* child);

    void unparent();

private: // Transform
    Transform transform{};
    glm::mat4 cachedWorldTransform{};
    bool isTransformDirty{true};

private: // Hierarchy
    GameObject* parent{nullptr};
    std::vector<GameObject *> children{};

private:
    static int nextId;
    int id;



public:
    bool operator==(const GameObject& other) const {
        return this->id == other.id;
    }
};


#endif //GAME_OBJECT_H
