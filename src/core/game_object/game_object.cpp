//
// Created by William on 2024-08-24.
//

#include "game_object.h"

#include "glm/gtc/quaternion.hpp"
#include "src/renderer/vk_types.h"
#include "src/renderer/render_object/render_object.h"

namespace will_engine
{
uint32_t GameObject::nextId = 0;

GameObject::GameObject() : gameObjectId(nextId++)
{
    gameObjectName = "GameObject_" + std::to_string(gameObjectId);
}

GameObject::GameObject(std::string gameObjectName) : gameObjectId(nextId++)
{
    if (gameObjectName == "") {
        this->gameObjectName = "GameObject_" + std::to_string(gameObjectId);
    } else {
        this->gameObjectName = std::move(gameObjectName);
    }

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

    if (!bodyId.IsInvalid()) {
        physics::Physics::Get()->removeRigidBody(this);
        bodyId = JPH::BodyID(JPH::BodyID::cInvalidBodyID);
    }
}

void GameObject::setName(std::string newName)
{
    if (gameObjectName == "") { return; }
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

void GameObject::setLocalPosition(const glm::vec3 localPosition, const bool isPhysics)
{
    transform.setPosition(localPosition);
    dirty();
    if (!isPhysics && !bodyId.IsInvalid()) {
        physics::Physics::Get()->getBodyInterface().SetPosition(bodyId, physics::physics_utils::ToJolt(getGlobalPosition()), JPH::EActivation::Activate);
        physics::Physics::Get()->getBodyInterface().SetRotation(bodyId, physics::physics_utils::ToJolt(getGlobalRotation()), JPH::EActivation::Activate);
    }
}

void GameObject::setLocalRotation(const glm::quat localRotation, const bool isPhysics)
{
    transform.setRotation(localRotation);
    dirty();
    if (!isPhysics && !bodyId.IsInvalid()) {
        physics::Physics::Get()->getBodyInterface().SetPosition(bodyId, physics::physics_utils::ToJolt(getGlobalPosition()), JPH::EActivation::Activate);
        physics::Physics::Get()->getBodyInterface().SetRotation(bodyId, physics::physics_utils::ToJolt(getGlobalRotation()), JPH::EActivation::Activate);
    }
}

void GameObject::setLocalScale(const glm::vec3 localScale, const bool isPhysics)
{
    transform.setScale(localScale);
    dirty();
    if (!isPhysics && !bodyId.IsInvalid()) {
        physics::Physics::Get()->getBodyInterface().SetPosition(bodyId, physics::physics_utils::ToJolt(getGlobalPosition()), JPH::EActivation::Activate);
        physics::Physics::Get()->getBodyInterface().SetRotation(bodyId, physics::physics_utils::ToJolt(getGlobalRotation()), JPH::EActivation::Activate);
    }
}

void GameObject::setLocalScale(const float localScale, const bool isPhysics)
{
    setLocalScale(glm::vec3(localScale), isPhysics);
}

void GameObject::setLocalTransform(const Transform& newLocalTransform, const bool isPhysics)
{
    transform = newLocalTransform;
    dirty();
    if (!isPhysics && !bodyId.IsInvalid()) {
        physics::Physics::Get()->getBodyInterface().SetPosition(bodyId, physics::physics_utils::ToJolt(getGlobalPosition()), JPH::EActivation::Activate);
        physics::Physics::Get()->getBodyInterface().SetRotation(bodyId, physics::physics_utils::ToJolt(getGlobalRotation()), JPH::EActivation::Activate);
    }
}

void GameObject::setGlobalPosition(const glm::vec3 globalPosition, const bool isPhysics)
{
    if (parent) {
        const glm::vec3 parentPos = parent->getGlobalPosition();
        const glm::quat parentRot = parent->getGlobalRotation();
        const glm::mat4 parentTransform = glm::translate(glm::mat4(1.0f), parentPos) * glm::mat4_cast(parentRot);
        const glm::mat4 inverseParentTransform = glm::inverse(parentTransform);
        const auto localPosition = glm::vec3(inverseParentTransform * glm::vec4(globalPosition, 1.0f));
        setLocalPosition(localPosition, isPhysics);
    } else {
        setLocalPosition(globalPosition, isPhysics);
    }
}

void GameObject::setGlobalRotation(const glm::quat globalRotation, const bool isPhysics)
{
    if (parent) {
        const Transform& parentGlobal = parent->getGlobalTransform();
        const glm::quat localRotation = glm::inverse(parentGlobal.getRotation()) * globalRotation;
        setLocalRotation(localRotation, isPhysics);
    } else {
        setLocalRotation(globalRotation, isPhysics);
    }
}

void GameObject::setGlobalScale(const glm::vec3 globalScale, const bool isPhysics)
{
    if (parent) {
        const Transform& parentGlobal = parent->getGlobalTransform();
        const glm::vec3 localScale = globalScale / parentGlobal.getScale();
        setLocalScale(localScale, isPhysics);
    } else {
        setLocalScale(globalScale, isPhysics);
    }
}

void GameObject::setGlobalScale(const float globalScale, const bool isPhysics)
{
    setGlobalScale(glm::vec3(globalScale), isPhysics);
}

void GameObject::setGlobalTransform(const Transform& newGlobalTransform, const bool isPhysics)
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
    if (!isPhysics && !bodyId.IsInvalid()) {
        physics::Physics::Get()->getBodyInterface().SetPosition(bodyId, physics::physics_utils::ToJolt(getGlobalPosition()), JPH::EActivation::Activate);
        physics::Physics::Get()->getBodyInterface().SetRotation(bodyId, physics::physics_utils::ToJolt(getGlobalRotation()), JPH::EActivation::Activate);
    }
}

void GameObject::translate(const glm::vec3 translation, const bool isPhysics)
{
    transform.translate(translation);
    dirty();
    if (!isPhysics && !bodyId.IsInvalid()) {
        physics::Physics::Get()->getBodyInterface().SetPosition(bodyId, physics::physics_utils::ToJolt(getGlobalPosition()), JPH::EActivation::Activate);
        physics::Physics::Get()->getBodyInterface().SetRotation(bodyId, physics::physics_utils::ToJolt(getGlobalRotation()), JPH::EActivation::Activate);
    }
}

void GameObject::rotate(const glm::quat rotation, const bool isPhysics)
{
    transform.rotate(rotation);
    dirty();
    if (!isPhysics && !bodyId.IsInvalid()) {
        physics::Physics::Get()->getBodyInterface().SetPosition(bodyId, physics::physics_utils::ToJolt(getGlobalPosition()), JPH::EActivation::Activate);
        physics::Physics::Get()->getBodyInterface().SetRotation(bodyId, physics::physics_utils::ToJolt(getGlobalRotation()), JPH::EActivation::Activate);
    }
}

void GameObject::rotateAxis(const float angle, const glm::vec3& axis, const bool isPhysics)
{
    transform.rotateAxis(angle, axis);
    dirty();
    if (!isPhysics && !bodyId.IsInvalid()) {
        physics::Physics::Get()->getBodyInterface().SetPosition(bodyId, physics::physics_utils::ToJolt(getGlobalPosition()), JPH::EActivation::Activate);
        physics::Physics::Get()->getBodyInterface().SetRotation(bodyId, physics::physics_utils::ToJolt(getGlobalRotation()), JPH::EActivation::Activate);
    }
}


void GameObject::dirty()
{
    bIsGlobalTransformDirty = true;
    framesToUpdate = FRAME_OVERLAP + 1;

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

GameObject* GameObject::getParent() const
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

    if (pRenderReference) {
        if (framesToUpdate > 0) {
            pRenderReference->updateInstanceData(instanceIndex, getModelMatrix());
            framesToUpdate--;
        }
    }

    for (GameObject* child : children) {
        child->recursiveUpdateModelMatrix();
    }
}

void GameObject::setupRigidbody(const JPH::ShapeRefC& shape, const JPH::EMotionType motionType, const JPH::ObjectLayer layer)
{
    const JPH::BodyCreationSettings settings{
        shape,
        physics::physics_utils::ToJolt(getGlobalPosition()),
        physics::physics_utils::ToJolt(getGlobalRotation()),
        motionType,
        layer
    };
    const auto physics = physics::Physics::Get();
    bodyId = physics->addRigidBody(this, settings);
}
}
