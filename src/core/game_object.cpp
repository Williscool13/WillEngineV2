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
    GameObject* tempParent = parent;
    // Remove Parent
    if (parent != nullptr) {
        reparent();
    }

    // Remove all children
    while (!children.empty()) {
        removeChild(children.back(), tempParent);
    }
}

void GameObject::setName(std::string newName)
{
    gameObjectName = std::move(newName);
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
    const glm::vec3 parentPos = parent->getGlobalPosition();
    const glm::quat parentRot = parent->getGlobalRotation();
    const glm::mat4 parentTransform = glm::translate(glm::mat4(1.0f), parentPos) * glm::mat4_cast(parentRot);
    const glm::mat4 inverseParentTransform = glm::inverse(parentTransform);
    const auto localPosition = glm::vec3(inverseParentTransform * glm::vec4(globalPosition, 1.0f));

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
        const glm::vec3 parentPos = parent->getGlobalPosition();
        const glm::quat parentRot = parent->getGlobalRotation();
        const glm::mat4 parentTransform = glm::translate(glm::mat4(1.0f), parentPos) * glm::mat4_cast(parentRot);
        const glm::mat4 inverseParentTransform = glm::inverse(parentTransform);
        const auto localPosition = glm::vec3(inverseParentTransform * glm::vec4(newGlobalTransform.getPosition(), 1.0f));

        const glm::quat localRotation = glm::inverse(parentRot) * newGlobalTransform.getRotation();
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
    framesToUpdate = 2;

    transform.setDirty();

    for (const auto& child : children) {
        child->dirty();
    }
}

void GameObject::addChild(GameObject* child, const bool maintainWorldTransform)
{
    // Prevent adding self as child
    if (child == this) { return; }

    // Remove from previous parent if any
    child->reparent(this, maintainWorldTransform);
    children.push_back(child);
}

void GameObject::removeChild(GameObject* child, GameObject* newParent)
{
    const auto it = std::ranges::find(children, child);
    if (it != children.end()) {
        // Done to prevent an infinite loop.
        GameObject* temp = *it;
        children.erase(it);
        temp->reparent(newParent);
    }
}

void GameObject::reparent(GameObject* newParent, const bool maintainWorldTransform)
{
    const Transform previousGlobalTransform = getGlobalTransform();
    if (parent) {
        // Done to prevent an infinite loop.
        GameObject* temp = parent;
        parent = nullptr;
        temp->removeChild(this);
    }

    parent = newParent;
    if (maintainWorldTransform) {
        setGlobalTransform(previousGlobalTransform);
    }
}

GameObject* GameObject::getParent()
{
    return parent;
}

std::vector<GameObject*>& GameObject::getChildren()
{
    return children;
}

void GameObject::recursiveUpdateModelMatrix(const int32_t previousFrameOverlapIndex, const int32_t currentFrameOverlapIndex)
{
    if (bIsStatic) {
        // if a gameobject is static, all its children must necessarily be static.
        return;
    }

    if (pRenderObject) {
        if (framesToUpdate > 0) {
            pRenderObject->updateInstanceData(instanceIndex, getModelMatrix(), previousFrameOverlapIndex, currentFrameOverlapIndex);
            framesToUpdate--;
        }
    }

    for (GameObject* child : children) {
        child->recursiveUpdateModelMatrix(previousFrameOverlapIndex, currentFrameOverlapIndex);
    }
}
