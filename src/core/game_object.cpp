//
// Created by William on 2024-08-24.
//

#include "game_object.h"

#include "glm/gtc/quaternion.hpp"

int GameObject::nextId = 0;

GameObject::GameObject()
    : gameObjectId(nextId++)
{}

GameObject::GameObject(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale)
    : gameObjectId(nextId++)
{
    transform.setPosition(position);
    transform.setRotation(rotation);
    transform.setScale(scale);
}

GameObject::~GameObject()
{
    // Remove Parent
    if (parent != nullptr) {
        unparent();
    }

    // Remove all children
    while (!children.empty()) {
        removeChild(children.back());
    }
}


glm::mat4 GameObject::getWorldMatrix()
{
    if (isTransformDirty) {
        if (parent != nullptr) {
            cachedWorldTransform = parent->getWorldMatrix() * transform.getWorldMatrix();
        } else {
            cachedWorldTransform = transform.getWorldMatrix();
        }
        isTransformDirty = false;
    }
    return cachedWorldTransform;
}

void GameObject::addChild(GameObject* child)
{
    // Prevent adding self as child
    if (child == this) { return; }

    // Child's current world transform
    glm::mat4 childWorldMatrix = child->getWorldMatrix();

    // Remove from previous parent if any
    child->unparent();
    children.push_back(child);
    child->parent = this;

    // Calculate and set the child's new local transform
    glm::mat4 inverseParentWorldMatrix = glm::inverse(this->getWorldMatrix());
    glm::mat4 newLocalMatrix = inverseParentWorldMatrix * childWorldMatrix;

    // Extract new local transform components
    glm::vec3 newLocalPosition = glm::vec3(newLocalMatrix[3]);
    glm::vec3 newLocalScale = glm::vec3(
        glm::length(glm::vec3(newLocalMatrix[0])),
        glm::length(glm::vec3(newLocalMatrix[1])),
        glm::length(glm::vec3(newLocalMatrix[2]))
    );
    glm::quat newLocalRotation = glm::quat_cast(newLocalMatrix);

    // Set the new local position, rotation, and scale
    child->transform.setPosition(newLocalPosition);
    child->transform.setRotation(glm::eulerAngles(newLocalRotation));
    child->transform.setScale(newLocalScale);
    child->setTransformDirty();
}

void GameObject::removeChild(GameObject* child)
{
    const auto it = std::ranges::find(children, child);
    if (it != children.end()) {
        // Done to prevent an infinite loop.
        GameObject* temp = *it;
        children.erase(it);
        temp->unparent();
    }
}

void GameObject::unparent()
{
    // Already unparented, nothing to do
    if (!parent) { return; }

    // Get the current world position, rotation, and scale
    glm::mat4 worldMatrix = getWorldMatrix();
    glm::vec3 worldPosition = glm::vec3(worldMatrix[3]);
    glm::vec3 worldScale = glm::vec3(
        glm::length(glm::vec3(worldMatrix[0])),
        glm::length(glm::vec3(worldMatrix[1])),
        glm::length(glm::vec3(worldMatrix[2]))
    );
    glm::quat worldRotation = glm::quat_cast(worldMatrix);

    // Done to prevent an infinite loop.
    GameObject* temp = parent;
    parent = nullptr;
    temp->removeChild(this);

    // Set the object's local transform to its previous world transform
    transform.setPosition(worldPosition);
    transform.setRotation(glm::eulerAngles(worldRotation)); // Convert quaternion to Euler angles
    transform.setScale(worldScale);
    setTransformDirty();
}
