//
// Created by William on 2024-08-24.
//

#include "game_object.h"

#include <imgui.h>

#include "glm/gtc/quaternion.hpp"
#include "components/component.h"
#include "components/rigid_body_component.h"
#include "src/core/engine.h"
#include "src/core/time.h"

namespace will_engine::game_object
{
uint64_t GameObject::runningTallyId = 0;

GameObject::GameObject(std::string gameObjectName)
{
    setId(runningTallyId++);
    if (gameObjectName.empty()) {
        this->gameObjectName = "GameObject_" + std::to_string(getId());
    }
    else {
        this->gameObjectName = std::move(gameObjectName);
    }

    if (will_engine::Engine* engine = will_engine::Engine::get()) {
        engine->addToBeginQueue(this);
    }
}

GameObject::~GameObject()
{
    if (!children.empty()) {
        fmt::print(
            "Error: GameObject destroyed with children, potentially not destroyed with ::destroy. This will result in orphaned children and null references");
    }

    if (parent) {
        fmt::print(
            "Error: GameObject destroyed with a parent, potentially not destroyed with ::destroy. This will result in orphaned children and null references");
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

    if (will_engine::Engine* engine = will_engine::Engine::get()) {
        engine->addToDeletionQueue(this);
    }
}

void GameObject::setName(std::string newName)
{
    if (newName == "") { return; }
    gameObjectName = std::move(newName);
}

void GameObject::beginPlay()
{
    for (const auto& component : components) {
        component->beginPlay();
    }
    bHasBegunPlay = true;
}

void GameObject::update(const float deltaTime)
{
    if (!bHasBegunPlay) { return; }
    for (const auto& component : components) {
        if (component->isEnabled()) {
            component->update(deltaTime);
        }
    }
}

void GameObject::beginDestroy()
{
    for (const auto& component : components) {
        component->beginDestroy();
    }
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
        parent->removeChild(this);
        setParent(nullptr);
    }

    if (newParent) {
        newParent->addChild(this);
    }

    setGlobalTransform(previousGlobalTransform);
}

void GameObject::dirty()
{
    if (meshRendererComponent) {
        meshRendererComponent->dirty();
    }

    bIsGlobalTransformDirty = true;
    if (rigidbodyComponent) {
        rigidbodyComponent->dirty();
    }

    for (const auto& child : children) {
        child->dirty();
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

glm::mat4 GameObject::getModelMatrix()
{
    getGlobalTransform();
    return cachedModelMatrix;
}

const Transform& GameObject::getGlobalTransform()
{
    if (bIsGlobalTransformDirty) {
        if (transformableParent != nullptr) {
            const Transform& parentGlobal = transformableParent->getGlobalTransform();
            const glm::vec3 globalPosition = parentGlobal.getPosition() + parentGlobal.getRotation() * (
                                                 parentGlobal.getScale() * transform.getPosition());
            const glm::quat globalRotation = parentGlobal.getRotation() * transform.getRotation();
            const glm::vec3 globalScale = parentGlobal.getScale() * transform.getScale();

            cachedGlobalTransform.setPosition(globalPosition);
            cachedGlobalTransform.setRotation(globalRotation);
            cachedGlobalTransform.setScale(globalScale);
            cachedModelMatrix = cachedGlobalTransform.toModelMatrix();
        }
        else {
            cachedGlobalTransform = transform;
            cachedModelMatrix = cachedGlobalTransform.toModelMatrix();
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

void GameObject::setLocalScale(const float localScale)
{
    setLocalScale(glm::vec3(localScale));
}

void GameObject::setLocalTransform(const Transform& newLocalTransform)
{
    transform = newLocalTransform;
    dirty();
}

void GameObject::setGlobalPosition(const glm::vec3 globalPosition)
{
    if (transformableParent) {
        const glm::vec3 parentPos = transformableParent->getPosition();
        const glm::quat parentRot = transformableParent->getRotation();
        const glm::mat4 parentTransform = glm::translate(glm::mat4(1.0f), parentPos) * glm::mat4_cast(parentRot);
        const glm::mat4 inverseParentTransform = glm::inverse(parentTransform);
        const auto localPosition = glm::vec3(inverseParentTransform * glm::vec4(globalPosition, 1.0f));
        setLocalPosition(localPosition);
    }
    else {
        setLocalPosition(globalPosition);
    }
}

void GameObject::setGlobalRotation(const glm::quat globalRotation)
{
    if (transformableParent) {
        const Transform& parentGlobal = transformableParent->getGlobalTransform();
        const glm::quat localRotation = glm::inverse(parentGlobal.getRotation()) * globalRotation;
        setLocalRotation(localRotation);
    }
    else {
        setLocalRotation(globalRotation);
    }
}

void GameObject::setGlobalScale(const glm::vec3 globalScale)
{
    if (transformableParent) {
        const Transform& parentGlobal = transformableParent->getGlobalTransform();
        const glm::vec3 localScale = globalScale / parentGlobal.getScale();
        setLocalScale(localScale);
    }
    else {
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
        const glm::vec3& parentPos = transformableParent->getPosition();
        const glm::quat& parentRot = transformableParent->getRotation();
        const glm::vec3& parentScale = transformableParent->getScale();

        const glm::vec3 relativePos = newGlobalTransform.getPosition() - parentPos;
        const glm::vec3 unrotatedPos = glm::inverse(parentRot) * relativePos;

        const glm::vec3 localPosition = unrotatedPos / parentScale;
        const glm::quat localRotation = glm::inverse(parentRot) * newGlobalTransform.getRotation();
        const glm::vec3 localScale = newGlobalTransform.getScale() / parentScale;

        transform.setTransform(localPosition, localRotation, localScale);
    }
    else {
        transform = newGlobalTransform;
    }

    dirty();
}

void GameObject::setGlobalTransformFromPhysics(const glm::vec3& position, const glm::quat& rotation)
{
    const glm::vec3 pos = getPosition();
    const glm::quat rot = getRotation();

    constexpr float positionEpsilonSquared = 0.000001f;
    constexpr float rotationEpsilon = 0.0001f;

    const float posDistSquared =
            (position.x - pos.x) * (position.x - pos.x) +
            (position.y - pos.y) * (position.y - pos.y) +
            (position.z - pos.z) * (position.z - pos.z);

    const bool positionUnchanged = posDistSquared < positionEpsilonSquared;
    const bool rotationUnchanged = glm::abs(dot(rotation, rot)) > (1.0f - rotationEpsilon);
    if (positionUnchanged && rotationUnchanged) { return; }


    if (transformableParent) {
        const glm::vec3 parentPos = transformableParent->getPosition();
        const glm::quat parentRot = transformableParent->getRotation();
        const glm::mat4 parentTransform = glm::mat4_cast(parentRot) * glm::translate(glm::mat4(1.0f), parentPos);
        const glm::mat4 inverseParentTransform = glm::inverse(parentTransform);
        const auto localPosition = glm::vec3(inverseParentTransform * glm::vec4(position, 1.0f));

        const glm::quat localRotation = glm::inverse(parentRot) * rotation;
        // Keep existing scale instead of computing new one
        const glm::vec3 currentScale = transform.getScale();

        transform.setTransform(localPosition, localRotation, currentScale);
    }
    else {
        transform.setPosition(position);
        transform.setRotation(rotation);
    }
    dirty();
}

void GameObject::translate(const glm::vec3 translation)
{
    transform.translate(translation);
    dirty();
}

void GameObject::rotate(const glm::quat rotation)
{
    transform.rotate(rotation);
    dirty();
}

void GameObject::rotateAxis(const float angle, const glm::vec3& axis)
{
    transform.rotateAxis(angle, axis);
    dirty();
}


components::Component* GameObject::getComponentByTypeName(std::string_view componentType)
{
    for (const auto& _component : components) {
        if (componentType == _component->getComponentType()) {
            return _component.get();
        }
    }

    return nullptr;
}

bool GameObject::hasComponent(std::string_view componentType)
{
    for (const auto& _component : components) {
        if (componentType == _component->getComponentType()) {
            return true;
        }
    }

    return false;
}

bool GameObject::canAddComponent(const std::string_view componentType)
{
    if (hasComponent(componentType)) {
        return false;
    }

    return true;
}

components::Component* GameObject::addComponent(std::unique_ptr<components::Component> component)
{
    if (!component) { return nullptr; }

    if (hasComponent(component->getComponentType())) {
        fmt::print("Attempted to add a component that already exists on this gameobject.\n");
        return nullptr;
    }

    component->setOwner(this);
    components.push_back(std::move(component));

    rigidbodyComponent = getComponent<components::RigidBodyComponent>();
    meshRendererComponent = getComponent<components::MeshRendererComponent>();

    if (bHasBegunPlay) {
        components.back()->beginPlay();
    }

    return components.back().get();
}

void GameObject::destroyComponent(components::Component* component)
{
    if (!component) { return; }

    const auto it = std::ranges::find_if(components, [component](const std::unique_ptr<components::Component>& comp) {
        return comp.get() == component;
    });

    if (it != components.end()) {
        component->beginDestroy();
        components.erase(it);
    }
    else {
        fmt::print("Attempted to remove a component that does not belong to this gameobject.\n");
    }

    rigidbodyComponent = nullptr;
    meshRendererComponent = nullptr;
    rigidbodyComponent = getComponent<components::RigidBodyComponent>();
    meshRendererComponent = getComponent<components::MeshRendererComponent>();
}

void GameObject::selectedRenderImgui()
{
    if (ImGui::Begin("Game Object")) {
        if (ImGui::BeginTabBar("Data")) {
            if (ImGui::BeginTabItem("Properties")) {
                ImGui::InputText("GameObject Name", &gameObjectName[0], gameObjectName.capacity() + 1);
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

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Components")) {
                static std::string failedComponentMessage;
                static float failedComponentTimeLeft{0};


                if (failedComponentTimeLeft > 0) {
                    if (!failedComponentMessage.empty()) {
                        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", failedComponentMessage.c_str());
                    }
                    failedComponentTimeLeft -= Time::Get().getDeltaTime();
                }

                if (ImGui::Button("Add Component")) {
                    ImGui::OpenPopup("AddComponentPopup");
                }

                if (ImGui::BeginPopup("AddComponentPopup")) {
                    ImGui::Text("Available Components:");
                    ImGui::Separator();

                    const auto& creators = components::ComponentFactory::getInstance().getManuallyCreatableComponentCreators();
                    for (const auto& type : creators | std::views::keys) {
                        if (ImGui::Selectable(type.data())) {
                            if (!canAddComponent(type)) {
                                failedComponentMessage = "Cannot add another " + std::string(type) + " component";
                                failedComponentTimeLeft = 7.5f;
                            }
                            else {
                                auto newComponent = components::ComponentFactory::getInstance().createComponent(type, "New " + std::string(type));
                                if (newComponent) {
                                    addComponent(std::move(newComponent));
                                }
                            }

                            ImGui::CloseCurrentPopup();
                        }
                    }
                    ImGui::EndPopup();
                }

                ImGui::Separator();

                components::Component* componentToDestroy{nullptr};
                for (auto& component : components) {
                    std::string& componentName = component->getComponentNameRef();
                    ImGui::PushID(component.get());

                    ImGui::Text("%s:", component->getComponentType().data());
                    ImGui::SameLine();

                    if (ImGui::Button("X", ImVec2(24, 0))) {
                        componentToDestroy = component.get();
                        ImGui::PopID();
                        continue;
                    }

                    ImGui::SameLine();


                    const char* componentType = component->getComponentType().data();
                    if (ImGui::Button(componentName.c_str(), ImVec2(-1, 0))) {
                        ImGui::OpenPopup(componentType);
                    }


                    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.75f, 0.75f, 0.75f, 1.0f)); // Dark gray
                    if (ImGui::BeginPopupModal(componentType, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                        ImGui::InputText("Component Name", &componentName[0], componentName.capacity() + 1);
                        ImGui::Separator();

                        component->updateRenderImgui();

                        ImGui::Separator();

                        constexpr float buttonWidth = 120.0f;
                        constexpr float deleteButtonWidth = 80.0f;

                        if (ImGui::Button("Close", ImVec2(buttonWidth, 0))) {
                            component->closeRenderImgui();
                            ImGui::CloseCurrentPopup();
                        }

                        float windowWidth = ImGui::GetContentRegionAvail().x;
                        ImGui::SameLine(windowWidth - deleteButtonWidth);

                        if (ImGui::Button("Delete", ImVec2(deleteButtonWidth, 0))) {
                            componentToDestroy = component.get();
                            ImGui::CloseCurrentPopup();
                        }

                        ImGui::EndPopup();
                    }
                    ImGui::PopStyleColor();

                    ImGui::PopID();
                }

                if (componentToDestroy) {
                    componentToDestroy->closeRenderImgui();
                    destroyComponent(componentToDestroy);
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }

    ImGui::End();
}
}
