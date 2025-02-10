//
// Created by William on 2024-08-24.
//

#include "game_object.h"

#include "glm/gtc/quaternion.hpp"
#include "src/core/identifier/identifier_manager.h"
#include "src/physics/physics.h"
#include "src/physics/physics_utils.h"

namespace will_engine
{
GameObject::GameObject(std::string gameObjectName, const uint64_t gameObjectId)
{
    GameObject::setId(identifier::IdentifierManager::Get()->registerIdentifier(gameObjectId));
    if (gameObjectName.empty()) {
        this->gameObjectName = "GameObject_" + std::to_string(GameObject::getId());
    } else {
        this->gameObjectName = std::move(gameObjectName);
    }
}

GameObject::~GameObject()
{
    for (const IHierarchical* child : children) {
        delete child;
    }

    if (parent) {
        parent->removeChild(this);
        parent = nullptr;
    }

    children.clear();

    if (pRenderReference) {
        pRenderReference->releaseInstanceIndex(instanceIndex);
        pRenderReference = nullptr;
    }

    if (bodyId != physics::BODY_ID_NONE) {
        if (physics::Physics* physics = physics::Physics::Get()) {
            physics->removeRigidBody(this);
            bodyId = physics::BODY_ID_NONE;
        }
    }
}

void GameObject::setName(std::string newName)
{
    if (newName == "") { return; }
    gameObjectName = std::move(newName);
}

bool GameObject::addChild(IHierarchical* child)
{
    if (child == this) { return false; }
    if (child->getParent() != nullptr) { return false; }
    if (std::ranges::find(children, child) != children.end()) { return false; }

    child->setParent(this);
    children.push_back(child);
    return true;
}

bool GameObject::removeChild(IHierarchical* child)
{
    if (child->getParent() != this) { return false; }

    const auto it = std::ranges::find(children, child);
    if (it == children.end()) { return false; }

    children.erase(it);
    child->setParent(nullptr);
    return true;
}

void GameObject::reparent(IHierarchical* newParent)
{
    if (this == newParent) { return; }
    const Transform previousGlobalTransform = getGlobalTransform();
    if (parent) {
        parent->removeChild(this);
    }
    parent = nullptr;
    if (newParent) {
        newParent->addChild(this);
    }
    parent = newParent;
    setGlobalTransform(previousGlobalTransform);
}

void GameObject::dirty()
{
    if (!bIsStatic) {
        bIsGlobalTransformDirty = true;
        framesToUpdate = FRAME_OVERLAP + 1;

        transform.setDirty();
    }

    for (const auto& child : children) {
        child->dirty();
    }
}

void GameObject::recursiveUpdate(const int32_t currentFrameOverlap, const int32_t previousFrameOverlap)
{
    if (pRenderReference && framesToUpdate > 0) {
        pRenderReference->updateInstanceData(instanceIndex, {getModelMatrix(), bIsVisible, bCastsShadows}, currentFrameOverlap, previousFrameOverlap);
        framesToUpdate--;
        return;
    }

    for (IHierarchical* child : children) {
        child->recursiveUpdate(currentFrameOverlap, previousFrameOverlap);
    }
}

void GameObject::setParent(IHierarchical* newParent)
{
    parent = newParent;
    transformableParent = dynamic_cast<ITransformable*>(newParent);
}

IHierarchical* GameObject::getParent() const
{
    return parent;
}

const std::vector<IHierarchical*>& GameObject::getChildren() const
{
    return children;
}

std::vector<IHierarchical*>& GameObject::getChildren()
{
    return children;
}

const Transform& GameObject::getGlobalTransform()
{
    if (bIsGlobalTransformDirty) {
        if (transformableParent != nullptr) {
            const Transform& parentGlobal = transformableParent->getGlobalTransform();
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
    if (bodyId != physics::BODY_ID_NONE) {
        physics::Physics::Get()->setPositionAndRotation(bodyId, getGlobalPosition(), getGlobalRotation());
    }
}

void GameObject::setLocalRotation(const glm::quat localRotation)
{
    transform.setRotation(localRotation);
    dirty();
    if (bodyId != physics::BODY_ID_NONE) {
        physics::Physics::Get()->setPositionAndRotation(bodyId, getGlobalPosition(), getGlobalRotation());
    }
}

void GameObject::setLocalScale(const glm::vec3 localScale)
{
    transform.setScale(localScale);
    dirty();
    if (bodyId != physics::BODY_ID_NONE) {
        physics::Physics::Get()->setPositionAndRotation(bodyId, getGlobalPosition(), getGlobalRotation());
    }
}

void GameObject::setLocalScale(const float localScale)
{
    setLocalScale(glm::vec3(localScale));
}

void GameObject::setLocalTransform(const Transform& newLocalTransform)
{
    transform = newLocalTransform;
    dirty();
    if (bodyId != physics::BODY_ID_NONE) {
        physics::Physics::Get()->setPositionAndRotation(bodyId, getGlobalPosition(), getGlobalRotation());
    }
}

void GameObject::setGlobalPosition(const glm::vec3 globalPosition)
{
    if (transformableParent) {
        const glm::vec3 parentPos = transformableParent->getGlobalPosition();
        const glm::quat parentRot = transformableParent->getGlobalRotation();
        const glm::mat4 parentTransform = glm::translate(glm::mat4(1.0f), parentPos) * glm::mat4_cast(parentRot);
        const glm::mat4 inverseParentTransform = glm::inverse(parentTransform);
        const auto localPosition = glm::vec3(inverseParentTransform * glm::vec4(globalPosition, 1.0f));
        setLocalPosition(localPosition);
    } else {
        setLocalPosition(globalPosition);
    }
}

void GameObject::setGlobalRotation(const glm::quat globalRotation)
{
    if (transformableParent) {
        const Transform& parentGlobal = transformableParent->getGlobalTransform();
        const glm::quat localRotation = glm::inverse(parentGlobal.getRotation()) * globalRotation;
        setLocalRotation(localRotation);
    } else {
        setLocalRotation(globalRotation);
    }
}

void GameObject::setGlobalScale(const glm::vec3 globalScale)
{
    if (transformableParent) {
        const Transform& parentGlobal = transformableParent->getGlobalTransform();
        const glm::vec3 localScale = globalScale / parentGlobal.getScale();
        setLocalScale(localScale);
    } else {
        setLocalScale(globalScale);
    }
}

void GameObject::setGlobalScale(const float globalScale)
{
    setGlobalScale(glm::vec3(globalScale));
}

void GameObject::setGlobalTransform(const Transform& newGlobalTransform)
{
    if (transformableParent) {
        const glm::vec3 parentPos = transformableParent->getGlobalPosition();
        const glm::quat parentRot = transformableParent->getGlobalRotation();
        const glm::mat4 parentTransform = glm::translate(glm::mat4(1.0f), parentPos) * glm::mat4_cast(parentRot);
        const glm::mat4 inverseParentTransform = glm::inverse(parentTransform);
        const auto localPosition = glm::vec3(inverseParentTransform * glm::vec4(newGlobalTransform.getPosition(), 1.0f));

        const glm::quat localRotation = glm::inverse(parentRot) * newGlobalTransform.getRotation();
        const glm::vec3 localScale = newGlobalTransform.getScale() / transformableParent->getGlobalScale();

        transform.setTransform(localPosition, localRotation, localScale);
    } else {
        transform = newGlobalTransform;
    }

    dirty();
    if (bodyId != physics::BODY_ID_NONE) {
        physics::Physics::Get()->setPositionAndRotation(bodyId, getGlobalPosition(), getGlobalRotation());
    }
}

void GameObject::translate(const glm::vec3 translation)
{
    transform.translate(translation);
    dirty();
    if (bodyId != physics::BODY_ID_NONE) {
        physics::Physics::Get()->setPositionAndRotation(bodyId, getGlobalPosition(), getGlobalRotation());
    }
}

void GameObject::rotate(const glm::quat rotation)
{
    transform.rotate(rotation);
    dirty();
    if (bodyId != physics::BODY_ID_NONE) {
        physics::Physics::Get()->setPositionAndRotation(bodyId, getGlobalPosition(), getGlobalRotation());
    }
}

void GameObject::rotateAxis(const float angle, const glm::vec3& axis)
{
    transform.rotateAxis(angle, axis);
    dirty();
    if (bodyId != physics::BODY_ID_NONE) {
        physics::Physics::Get()->setPositionAndRotation(bodyId, getGlobalPosition(), getGlobalRotation());
    }
}

void GameObject::setGlobalTransformFromPhysics(const glm::vec3& position, const glm::quat& rotation)
{
    if (transformableParent) {
        const Transform& parentGlobal = transformableParent->getGlobalTransform();
        const glm::mat4 parentTransform = glm::translate(glm::mat4(1.0f), parentGlobal.getPosition()) * glm::mat4_cast(parentGlobal.getRotation());
        transform.setPosition(glm::vec3(glm::inverse(parentTransform) * glm::vec4(position, 1.0f)));
        transform.setRotation(glm::inverse(parentGlobal.getRotation()) * rotation);
    } else {
        transform.setPosition(position);
        transform.setRotation(rotation);
    }
    dirty();
}
}
