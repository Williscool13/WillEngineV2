//
// Created by William on 2024-08-24.
//

#include "game_object.h"

#include "glm/gtc/quaternion.hpp"
#include "src/renderer/vk_types.h"
#include "src/renderer/render_object/render_object.h"

uint32_t GameObject::nextId = 0;

GameObject::GameObject() : gameObjectId(nextId++)
{
    gameObjectName = "GameObject_" + std::to_string(gameObjectId);
}

GameObject::GameObject(std::string gameObjectName) : gameObjectId(nextId++)
{
    this->gameObjectName = std::move(gameObjectName);
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

const Transform& GameObject::getGlobalTransform()
{
    if (bIsGlobalTransformDirty) {
        if (parent) {
            const Transform& parentGlobal = parent->getGlobalTransform();
            cachedGlobalTransform.setPosition(parentGlobal.getPositionMatrix() * glm::vec4(transform.getPosition(), 1.0f));
            cachedGlobalTransform.setRotation(parentGlobal.getRotation() * transform.getRotation());
            cachedGlobalTransform.setScale(parentGlobal.getScale() * transform.getScale());
        } else {
            cachedGlobalTransform = transform;
        }
        bIsGlobalTransformDirty = false;
    }

    return cachedGlobalTransform;
}


void GameObject::setLocalPosition(const glm::vec3 localPosition)
{
    transform.setPosition(localPosition);
    dirty();
}

void GameObject::setLocalRotation(const glm::quat localRotation)
{
    transform.setRotation(localRotation);
    dirty();
}

void GameObject::setLocalScale(const glm::vec3 localScale)
{
    transform.setScale(localScale);
    dirty();
}

void GameObject::setLocalTransform(const Transform& newLocalTransform)
{
    transform = newLocalTransform;
    dirty();
}

void GameObject::setGlobalPosition(const glm::vec3 globalPosition)
{
    const glm::mat4 inverseParentMatrix = glm::inverse(parent->getModelMatrix());
    const glm::vec3 localPosition = inverseParentMatrix * glm::vec4(globalPosition, 1.0f);
    setLocalPosition(localPosition);
}

void GameObject::setGlobalRotation(const glm::quat globalRotation)
{
    const Transform& parentGlobal = parent->getGlobalTransform();
    const glm::quat localRotation = glm::inverse(parentGlobal.getRotation()) * globalRotation;
    setLocalRotation(localRotation);
}

void GameObject::setGlobalScale(const glm::vec3 globalScale)
{
    const Transform& parentGlobal = parent->getGlobalTransform();
    const glm::vec3 localScale = globalScale / parentGlobal.getScale();
    setLocalScale(localScale);
}

void GameObject::setGlobalTransform(const Transform& newGlobalTransform)
{
    if (parent) {
        const glm::mat4 inverseParentMatrix = glm::inverse(parent->getModelMatrix());

        const glm::vec3 localPosition = inverseParentMatrix * glm::vec4(newGlobalTransform.getPosition(), 1.0f);
        const glm::quat localRotation = glm::inverse(parent->getGlobalRotation()) * newGlobalTransform.getRotation();
        const glm::vec3 localScale = newGlobalTransform.getScale() / parent->getGlobalScale();

        transform.setTransform(localPosition, localRotation, localScale);
    } else {
        transform = newGlobalTransform;
    }

    dirty();
}


void GameObject::dirty()
{
    bIsGlobalTransformDirty = true;
    bModelPendingUpdate = true;

    transform.setDirty();

    for (const auto& child : children) {
        child->dirty();
    }
}

void GameObject::addChild(GameObject* child, bool maintainWorldPosition)
{
    // Prevent adding self as child
    if (child == this) { return; }

    // Remove from previous parent if any
    child->unparent();
    children.push_back(child);
    child->parent = this;

    if (maintainWorldPosition) {
        // Child's current world transform
        const glm::mat4 childWorldMatrix = child->getModelMatrix();

        // Calculate and set the child's new local transform
        const glm::mat4 inverseParentWorldMatrix = glm::inverse(this->getModelMatrix());
        glm::mat4 newLocalMatrix = inverseParentWorldMatrix * childWorldMatrix;

        // Extract new local transform components
        const auto newLocalPosition = glm::vec3(newLocalMatrix[3]);
        const auto newLocalScale = glm::vec3(
            glm::length(glm::vec3(newLocalMatrix[0])),
            glm::length(glm::vec3(newLocalMatrix[1])),
            glm::length(glm::vec3(newLocalMatrix[2]))
        );
        glm::quat newLocalRotation = glm::quat_cast(newLocalMatrix);

        // Set the new local position, rotation, and scale
        child->transform.setTransform(newLocalPosition, newLocalRotation, newLocalScale);
        child->dirty();
    }
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
    glm::mat4 worldMatrix = getModelMatrix();
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
    transform.setRotation(worldRotation);
    transform.setScale(worldScale);
    dirty();
}

GameObject* GameObject::getParent()
{
    return parent;
}

std::vector<GameObject*>& GameObject::getChildren()
{
    return children;
}

void GameObject::recursiveUpdateModelMatrix()
{
    if (bIsStatic) {
        // if a gameobject is static, all its children must necessarily be static.
        return;
    }

    if (pRenderObject) {
        if (InstanceData* pInstanceData = pRenderObject->getInstanceData(instanceIndex)) {
            if (bModelPendingUpdate) {
                if (bModelUpdatedLastFrame) {
                    pInstanceData->previousModelMatrix = pInstanceData->currentModelMatrix;
                }
                //pInstanceData->previousModelMatrix = pInstanceData->currentModelMatrix;
                pInstanceData->currentModelMatrix = getModelMatrix();
                bModelUpdatedLastFrame = true;
            } else if (bModelUpdatedLastFrame) {
                pInstanceData->previousModelMatrix = pInstanceData->currentModelMatrix;
                bModelUpdatedLastFrame = false;
            }
        }
    }

    for (GameObject* child : children) {
        child->recursiveUpdateModelMatrix();
    }
}
