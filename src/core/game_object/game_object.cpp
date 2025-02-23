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
#include "src/core/identifier/identifier_manager.h"
#include "src/util/math_utils.h"

namespace will_engine
{
GameObject::GameObject(std::string gameObjectName, const uint64_t gameObjectId)
{
    GameObject::setId(identifier::IdentifierManager::Get()->registerIdentifier(gameObjectId));
    if (gameObjectName.empty()) {
        this->gameObjectName = "GameObject_" + std::to_string(GameObject::getId());
    }
    else {
        this->gameObjectName = std::move(gameObjectName);
    }

    if (Engine* engine = Engine::get()) {
        engine->addToBeginQueue(this);
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
    transform.setDirty();

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

const Transform& GameObject::getGlobalTransform()
{
    if (bIsGlobalTransformDirty) {
        if (transformableParent != nullptr) {
            const Transform& parentGlobal = transformableParent->getGlobalTransform();
            cachedGlobalTransform.setPosition(parentGlobal.getPositionMatrix() * glm::vec4(transform.getPosition(), 1.0f));
            cachedGlobalTransform.setRotation(parentGlobal.getRotation() * transform.getRotation());
            cachedGlobalTransform.setScale(parentGlobal.getScale() * transform.getScale());
        }
        else {
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
    if (rigidbodyComponent) {
        rigidbodyComponent->setPhysicsTransformFromGame(getGlobalPosition(), getGlobalRotation());
    }
}

void GameObject::setLocalRotation(const glm::quat localRotation)
{
    transform.setRotation(localRotation);
    dirty();
    if (rigidbodyComponent) {
        rigidbodyComponent->setPhysicsTransformFromGame(getGlobalPosition(), getGlobalRotation());
    }
}

void GameObject::setLocalScale(const glm::vec3 localScale)
{
    transform.setScale(localScale);
    dirty();
    if (rigidbodyComponent) {
        rigidbodyComponent->setPhysicsTransformFromGame(getGlobalPosition(), getGlobalRotation());
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
    if (rigidbodyComponent) {
        rigidbodyComponent->setPhysicsTransformFromGame(getGlobalPosition(), getGlobalRotation());
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
        const glm::vec3 parentPos = transformableParent->getGlobalPosition();
        const glm::quat parentRot = transformableParent->getGlobalRotation();
        const glm::mat4 parentTransform = glm::mat4_cast(parentRot) * glm::translate(glm::mat4(1.0f), parentPos);
        const glm::mat4 inverseParentTransform = glm::inverse(parentTransform);
        const auto localPosition = glm::vec3(inverseParentTransform * glm::vec4(newGlobalTransform.getPosition(), 1.0f));

        const glm::quat localRotation = glm::inverse(parentRot) * newGlobalTransform.getRotation();
        const glm::vec3 localScale = newGlobalTransform.getScale() / transformableParent->getGlobalScale();

        transform.setTransform(localPosition, localRotation, localScale);
    }
    else {
        transform = newGlobalTransform;
    }

    dirty();
    if (rigidbodyComponent) {
        rigidbodyComponent->setPhysicsTransformFromGame(getGlobalPosition(), getGlobalRotation());
    }
}

void GameObject::setGlobalTransformFromPhysics(const glm::vec3& position, const glm::quat& rotation)
{
    if (transformableParent) {
        const glm::vec3 parentPos = transformableParent->getGlobalPosition();
        const glm::quat parentRot = transformableParent->getGlobalRotation();
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
    if (rigidbodyComponent) {
        rigidbodyComponent->setPhysicsTransformFromGame(getGlobalPosition(), getGlobalRotation());
    }
}

void GameObject::rotate(const glm::quat rotation)
{
    transform.rotate(rotation);
    dirty();
    if (rigidbodyComponent) {
        rigidbodyComponent->setPhysicsTransformFromGame(getGlobalPosition(), getGlobalRotation());
    }
}

void GameObject::rotateAxis(const float angle, const glm::vec3& axis)
{
    transform.rotateAxis(angle, axis);
    dirty();
    if (rigidbodyComponent) {
        rigidbodyComponent->setPhysicsTransformFromGame(getGlobalPosition(), getGlobalRotation());
    }
}


bool GameObject::canAddComponent(const std::string_view componentType)
{
    for (const auto& _component : components) {
        if (componentType == _component->getComponentType()) {
            fmt::print("Attempted to add a component of the same type to a gameobject. This is not supported at this time.\n");
            return false;
        }
    }

    return true;
}

components::Component* GameObject::addComponent(std::unique_ptr<components::Component> component)
{
    if (!component) { return nullptr; }

    for (const auto& _component : components) {
        if (component->getComponentType() == _component->getComponentType()) {
            fmt::print("Attempted to add a component of the same type to a gameobject. This is not supported at this time.\n");
            return nullptr;
        }
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

                    const auto& creators = components::ComponentFactory::getInstance().getComponentCreators();
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
                    auto componentName = std::string(component->getComponentName());
                    if (componentName.empty()) {
                        componentName = "<unnamed>";
                    }

                    ImGui::PushID(component.get());

                    ImGui::Text("%s:", component->getComponentType().data());
                    ImGui::SameLine();

                    if (ImGui::Button("X", ImVec2(24, 0))) {
                        componentToDestroy = component.get();
                        ImGui::PopID();
                        continue;
                    }

                    ImGui::SameLine();

                    static char objectName[256] = "";

                    const char* componentType = component->getComponentType().data();
                    if (ImGui::Button(componentName.c_str(), ImVec2(-1, 0))) {
                        component->openRenderImgui();
                        strncpy_s(objectName, sizeof(objectName), componentName.c_str(), _TRUNCATE);
                        ImGui::OpenPopup(componentType);
                    }


                    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.75f, 0.75f, 0.75f, 1.0f)); // Dark gray
                    if (ImGui::BeginPopupModal(componentType, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                        if (ImGui::InputText("Component Name", objectName, sizeof(objectName))) {
                            // ReSharper disable once CppDFAUnusedValue
                            componentName = objectName;
                        }
                        component->setComponentName(objectName);
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
