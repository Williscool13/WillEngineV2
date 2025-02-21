//
// Created by William on 2024-08-24.
//

#include "game_object.h"

#include <imgui.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include "glm/gtc/quaternion.hpp"
#include "components/component.h"
#include "src/core/engine.h"
#include "src/core/identifier/identifier_manager.h"
#include "src/physics/physics.h"
#include "src/physics/physics_filters.h"
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
    for (int32_t i = children.size() - 1; i >= 0; --i) {
        delete children[i];
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

    if (bodyId.GetIndex() != JPH::BodyID::cMaxBodyIndex) {
        if (physics::Physics* physics = physics::Physics::Get()) {
            physics->removeRigidBody(this);
            bodyId = JPH::BodyID(JPH::BodyID::cMaxBodyIndex);
        }
    }
}

void GameObject::destroy()
{
    for (IHierarchical* child : children) {
        child->removeParent();
    }

    if (parent) {
        for (IHierarchical* child : children) {
            child->reparent(parent);
        }
        parent->removeChild(this);
    }

    children.clear();

    if (Engine* engine = Engine::get()) {
        engine->addGameObjectToDeletionQueue(this);
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

bool GameObject::removeParent()
{
    if (parent == nullptr) { return false; }
    const Transform previousGlobalTransform = getGlobalTransform();
    setParent(nullptr);
    setGlobalTransform(previousGlobalTransform);
    return true;
}

void GameObject::reparent(IHierarchical* newParent)
{
    if (this == newParent) { return; }
    const Transform previousGlobalTransform = getGlobalTransform();
    if (parent) {
        if (!parent->removeChild(this)) {
            setParent(nullptr);
        }
    }

    if (newParent) {
        newParent->addChild(this);
    } else {
        setParent(newParent);
    }
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
        pRenderReference->updateInstanceData(instanceIndex, {getModelMatrix(), bIsVisible, bIsShadowCaster}, currentFrameOverlap, previousFrameOverlap);
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
    if (bodyId.GetIndex() != JPH::BodyID::cMaxBodyIndex) {
        physics::Physics::Get()->setPositionAndRotation(bodyId, getGlobalPosition(), getGlobalRotation());
    }
}

void GameObject::setLocalRotation(const glm::quat localRotation)
{
    transform.setRotation(localRotation);
    dirty();
    if (bodyId.GetIndex() != JPH::BodyID::cMaxBodyIndex) {
        physics::Physics::Get()->setPositionAndRotation(bodyId, getGlobalPosition(), getGlobalRotation());
    }
}

void GameObject::setLocalScale(const glm::vec3 localScale)
{
    transform.setScale(localScale);
    dirty();
    if (bodyId.GetIndex() != JPH::BodyID::cMaxBodyIndex) {
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
    if (bodyId.GetIndex() != JPH::BodyID::cMaxBodyIndex) {
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
    if (bodyId.GetIndex() != JPH::BodyID::cMaxBodyIndex) {
        physics::Physics::Get()->setPositionAndRotation(bodyId, getGlobalPosition(), getGlobalRotation());
    }
}

void GameObject::translate(const glm::vec3 translation)
{
    transform.translate(translation);
    dirty();
    if (bodyId.GetIndex() != JPH::BodyID::cMaxBodyIndex) {
        physics::Physics::Get()->setPositionAndRotation(bodyId, getGlobalPosition(), getGlobalRotation());
    }
}

void GameObject::rotate(const glm::quat rotation)
{
    transform.rotate(rotation);
    dirty();
    if (bodyId.GetIndex() != JPH::BodyID::cMaxBodyIndex) {
        physics::Physics::Get()->setPositionAndRotation(bodyId, getGlobalPosition(), getGlobalRotation());
    }
}

void GameObject::rotateAxis(const float angle, const glm::vec3& axis)
{
    transform.rotateAxis(angle, axis);
    dirty();
    if (bodyId.GetIndex() != JPH::BodyID::cMaxBodyIndex) {
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

void GameObject::addComponent(std::unique_ptr<components::Component> component)
{
    if (!component) { return; }

    for (const auto& _component : components) {
        if (component->getComponentType() == _component->getComponentType()) {
            fmt::print("Attempted to add a component of the same type to a gameobject. This is not supported at this time.");
            return;
        }
    }
    components.push_back(std::move(component));

    if (bHasBegunPlay) {
        components.back()->beginPlay(this);
    }
}

void GameObject::destroyComponent()
{}

void GameObject::selectedRenderImgui()
{
    if (ImGui::Begin("Game Object")) {
        if (ImGui::BeginTabBar("Data")) {
            if (ImGui::BeginTabItem("Properties")) {
                ImGui::Text("Name: %s", getName().data());
                ImGui::Separator();

                // ITransformable
                if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                    glm::vec3 position = getLocalPosition();
                    if (ImGui::DragFloat3("Position", &position.x, 0.1f)) {
                        setLocalPosition(position);
                    }

                    glm::vec3 eulerAngles = glm::degrees(glm::eulerAngles(getLocalRotation()));
                    if (ImGui::DragFloat3("Rotation", &eulerAngles.x, 1.0f)) {
                        glm::quat newRotation = glm::quat(glm::radians(eulerAngles));
                        setLocalRotation(newRotation);
                    }

                    glm::vec3 scale = getLocalScale();
                    if (ImGui::DragFloat3("Scale", &scale.x, 0.1f, 0.01f)) {
                        setLocalScale(scale);
                    }
                }

                // IRenderable
                if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
                    bool _isVisible = isVisible();
                    if (ImGui::Checkbox("Visible", &_isVisible)) {
                        setVisibility(_isVisible);
                    }

                    bool castsShadows = isShadowCaster();
                    if (ImGui::Checkbox("Cast Shadows", &castsShadows)) {
                        setIsShadowCaster(castsShadows);
                    }
                }

                // IPhysicsBody
                if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
                    const JPH::BodyID bodyId = getPhysicsBodyId();

                    if (bodyId.GetIndex() == JPH::BodyID::cMaxBodyIndex) {
                        static int selectedShape = 0;
                        const char* shapes[] = {"Box", "Sphere", "Capsule", "Cylinder"};
                        ImGui::Combo("Shape", &selectedShape, shapes, 4);
                        static bool useUnitShape = true;
                        glm::vec3 shapeParams = glm::vec3(1.0f);

                        // Shape Generation
                        {
                            ImGui::Checkbox("Use Unit Shape", &useUnitShape);
                            if (!useUnitShape) {
                                switch (selectedShape) {
                                    case 0: // Box
                                    {
                                        static glm::vec3 boxHalfExtents(1.0f);
                                        ImGui::Text("Half Extents");
                                        ImGui::DragFloat3("##BoxHalfExtents", &boxHalfExtents.x, 0.1f, 0.1f, 100.0f);
                                        shapeParams = boxHalfExtents;
                                        break;
                                    }
                                    case 1: // Sphere
                                    {
                                        static float radius = 1.0f;
                                        ImGui::Text("Radius");
                                        ImGui::DragFloat("##SphereRadius", &radius, 0.1f, 0.1f, 100.0f);
                                        shapeParams = glm::vec3(radius, 0.0f, 0.0f);
                                        break;
                                    }
                                    case 2: // Capsule
                                    {
                                        static float radius = 1.0f;
                                        static float halfHeight = 1.0f;
                                        ImGui::Text("Radius");
                                        ImGui::DragFloat("##CapsuleRadius", &radius, 0.1f, 0.1f, 100.0f);
                                        ImGui::Text("Half Height");
                                        ImGui::DragFloat("##CapsuleHeight", &halfHeight, 0.1f, 0.1f, 100.0f);
                                        shapeParams = glm::vec3(radius, halfHeight, 0.0f);
                                        break;
                                    }
                                    case 3: // Cylinder
                                    {
                                        static float radius = 1.0f;
                                        static float halfHeight = 1.0f;
                                        ImGui::Text("Radius");
                                        ImGui::DragFloat("##CylinderRadius", &radius, 0.1f, 0.1f, 100.0f);
                                        ImGui::Text("Half Height");
                                        ImGui::DragFloat("##CylinderHeight", &halfHeight, 0.1f, 0.1f, 100.0f);
                                        shapeParams = glm::vec3(radius, halfHeight, 0.0f);
                                        break;
                                    }
                                    default:
                                        shapeParams = glm::vec3(1.0f);
                                        break;
                                }
                            } else {
                                switch (selectedShape) {
                                    case 0:
                                        shapeParams = physics::UNIT_CUBE;
                                        break;
                                    case 1:
                                        shapeParams = physics::UNIT_SPHERE;
                                        break;
                                    case 2:
                                        shapeParams = physics::UNIT_CAPSULE;
                                        break;
                                    case 3:
                                        shapeParams = physics::UNIT_CYLINDER;
                                        break;
                                    default:
                                        shapeParams = glm::vec3(1.0f);
                                        break;
                                }
                            }
                        }

                        static auto motionType = JPH::EMotionType::Dynamic;
                        const char* motionTypes[] = {"Static", "Kinematic", "Dynamic"};
                        int currentType = static_cast<int>(motionType);
                        ImGui::Combo("Motion Type", &currentType, motionTypes, 3);
                        motionType = static_cast<JPH::EMotionType>(currentType);

                        const char* layers[] = {"Non-Moving", "Moving", "Player", "Terrain"};
                        static JPH::ObjectLayer layer = physics::Layers::MOVING;
                        int currentLayer = layer;
                        ImGui::Combo("Layer", &currentLayer, layers, 4);
                        layer = static_cast<JPH::ObjectLayer>(currentLayer);


                        if (ImGui::Button("Add Rigidbody")) {
                            JPH::EShapeSubType shapeType;

                            switch (selectedShape) {
                                case 0:
                                    shapeType = JPH::EShapeSubType::Box;
                                    break;
                                case 1:
                                    shapeType = JPH::EShapeSubType::Sphere;
                                    break;
                                case 2:
                                    shapeType = JPH::EShapeSubType::Capsule;
                                    break;
                                case 3:
                                    shapeType = JPH::EShapeSubType::Cylinder;
                                    break;
                                default: shapeType = JPH::EShapeSubType::Box;
                                    break;
                            }

                            physics::Physics::Get()->setupRigidbody(
                                this,
                                shapeType,
                                shapeParams,
                                motionType,
                                layer
                            );
                        }
                    } else {
                        ImGui::Text("Body Id: %u", bodyId);

                        ImGui::Separator();

                        // Layer
                        {
                            JPH::ObjectLayer layer = physics::PhysicsUtils::getObjectLayer(bodyId);
                            if (layer < physics::Layers::NUM_LAYERS) {
                                auto layerName = physics::Layers::layerNames[layer];
                                ImGui::Text("Layer: %s", layerName);
                            } else {
                                ImGui::Text("Layer: Invalid Layer Found greater than NUM_LAYERS");
                            }
                        }

                        ImGui::Separator();

                        // Shape
                        {
                            if (physics::PhysicsObject* physicsObject = physics::Physics::Get()->getPhysicsObject(bodyId)) {
                                const JPH::ShapeRefC shape = physicsObject->shape;
                                ImGui::Text("Shape: ");
                                ImGui::SameLine();

                                switch (physicsObject->shape->GetSubType()) {
                                    case JPH::EShapeSubType::Box:
                                    {
                                        const auto box = static_cast<const JPH::BoxShape*>(shape.GetPtr());
                                        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Box");
                                        const JPH::Vec3 halfExtent = box->GetHalfExtent();
                                        ImGui::Columns(2);
                                        ImGui::Text("Half Extents");
                                        ImGui::NextColumn();
                                        ImGui::Text("X: %.2f  Y: %.2f  Z: %.2f", halfExtent.GetX(), halfExtent.GetY(), halfExtent.GetZ());
                                        ImGui::Columns(1);
                                        break;
                                    }
                                    case JPH::EShapeSubType::Sphere:
                                    {
                                        const auto sphere = static_cast<const JPH::SphereShape*>(shape.GetPtr());
                                        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Sphere");
                                        ImGui::Columns(2);
                                        ImGui::Text("Radius");
                                        ImGui::NextColumn();
                                        ImGui::Text("%.2f", sphere->GetRadius());
                                        ImGui::Columns(1);
                                        break;
                                    }
                                    case JPH::EShapeSubType::Capsule:
                                    {
                                        const auto capsule = static_cast<const JPH::CapsuleShape*>(shape.GetPtr());
                                        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Capsule");
                                        ImGui::Columns(2);
                                        ImGui::Text("Radius");
                                        ImGui::NextColumn();
                                        ImGui::Text("%.2f", capsule->GetRadius());
                                        ImGui::NextColumn();
                                        ImGui::Text("Half Height");
                                        ImGui::NextColumn();
                                        ImGui::Text("%.2f", capsule->GetHalfHeightOfCylinder());
                                        ImGui::Columns(1);
                                        break;
                                    }
                                    case JPH::EShapeSubType::Cylinder:
                                    {
                                        const auto cylinder = static_cast<const JPH::CylinderShape*>(shape.GetPtr());
                                        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Cylinder");
                                        ImGui::Columns(2);
                                        ImGui::Text("Radius");
                                        ImGui::NextColumn();
                                        ImGui::Text("%.2f", cylinder->GetRadius());
                                        ImGui::NextColumn();
                                        ImGui::Text("Half Height");
                                        ImGui::NextColumn();
                                        ImGui::Text("%.2f", cylinder->GetHalfHeight());
                                        ImGui::Columns(1);
                                        break;
                                    }
                                    default:
                                        ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.4f, 1.0f), "Unknown");
                                        break;
                                }
                            }
                        }

                        ImGui::Separator();

                        // MotionType
                        {
                            JPH::EMotionType currentMotionType = physics::Physics::Get()->getMotionType(this);
                            if (currentMotionType == JPH::EMotionType::Static) {
                                ImGui::Text("Motion Type: Static (Unable to change)");
                            } else {
                                const char* motionTypes[] = {"Kinematic", "Dynamic"};
                                int currentType = static_cast<int>(currentMotionType) - 1;

                                if (ImGui::Combo("Motion Type", &currentType, motionTypes, 2)) {
                                    const JPH::EMotionType newMotionType = static_cast<JPH::EMotionType>(currentType + 1);
                                    switch (newMotionType) {
                                        case JPH::EMotionType::Kinematic:
                                            physics::Physics::Get()->setMotionType(
                                                this,
                                                static_cast<JPH::EMotionType>(currentType + 1),
                                                JPH::EActivation::DontActivate
                                            );
                                            break;
                                        case JPH::EMotionType::Dynamic:
                                            physics::Physics::Get()->setMotionType(
                                                this,
                                                static_cast<JPH::EMotionType>(currentType + 1),
                                                JPH::EActivation::Activate
                                            );
                                            break;
                                        default:
                                            break;
                                    }
                                    physics::PhysicsUtils::resetVelocity(bodyId);
                                }

                                glm::vec3 velocity = physics::PhysicsUtils::getLinearVelocity(bodyId);
                                ImGui::Text("Velocity: %.2f, %.2f, %.2f", velocity.x, velocity.y, velocity.z);

                                if (ImGui::Button("Reset Velocity")) {
                                    physics::PhysicsUtils::resetVelocity(bodyId);
                                }


                                if (ImGui::Button("Remove Rigidbody")) {
                                    physics::Physics::Get()->releaseRigidbody(this);
                                }
                            }
                        }
                    }
                }

                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Components")) {
                for (auto& component : components) {
                    auto headerName = std::string(component->getComponentName());
                    if (ImGui::CollapsingHeader(headerName.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                        component->selectedRenderImgui();
                    }
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}
}
