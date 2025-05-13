//
// Created by William on 2024-08-24.
//

#ifndef GAME_OBJECT_H
#define GAME_OBJECT_H

#include <string>

#include <glm/glm.hpp>

#include "component_container.h"
#include "hierarchical.h"
#include "imgui_renderable.h"
#include "transformable.h"
#include "engine/core/transform.h"
#include "engine/core/game_object/components/component.h"
#include "engine/util/math_constants.h"

namespace will_engine::components
{
class MeshRendererComponent;
class RigidBodyComponent;
}

namespace will_engine::game_object
{
class Engine;
class RenderObject;

class GameObject : public ITransformable,
                   public IHierarchical,
                   public IImguiRenderable,
                   public IComponentContainer
{
public:
    explicit GameObject(std::string gameObjectName = "");

    ~GameObject() override;

    void destroy() override;

protected:
    bool bHasBegunPlay{false};

public: // IIdentifiable
    void setId(const uint64_t identifier) { gameObjectId = identifier; }

    uint64_t getId() const { return gameObjectId; }

private: // IIdentifiable
    uint64_t gameObjectId{};

    static uint64_t runningTallyId;

public: // IHierarchical
    void beginPlay() override;

    void update(float deltaTime) override;

    void beginDestroy() override;

    bool addChild(IHierarchical* child) override;

    bool removeChild(IHierarchical* child) override;

    /**
     * Removes the parent while maintaining world position. Does not attempt to update the parent's state.
     * \n WARNING: Doing this without re-parenting will result in an orphaned gameobject, beware.
     * @return
     */
    bool removeParent() override;

    void reparent(IHierarchical* newParent) override;

    void dirty() override;

    void setParent(IHierarchical* newParent) override;

    IHierarchical* getParent() const override;

    const std::vector<IHierarchical*>& getChildren() const override;

    std::vector<IHierarchical*>& getChildren() override;

    void setName(std::string newName) override;

    std::string_view getName() const override { return gameObjectName; }

protected: // IHierarchical
    IHierarchical* parent{nullptr};
    /**
     * Cache parent's transformable interface to reduce excessive casting
     */
    ITransformable* transformableParent{nullptr};
    std::vector<IHierarchical*> children{};
    std::string gameObjectName{};

public: // ITransformable
    glm::mat4 getModelMatrix() override;

    const Transform& getLocalTransform() const override { return transform; }
    glm::vec3 getLocalPosition() const override { return getLocalTransform().getPosition(); }
    glm::quat getLocalRotation() const override { return getLocalTransform().getRotation(); }
    glm::vec3 getLocalScale() const override { return getLocalTransform().getScale(); }

    const Transform& getGlobalTransform() override;

    /**
     * Interface of both IPhysicsBody and ITransformable
     * @return
     */
    glm::vec3 getPosition() override { return getGlobalTransform().getPosition(); }
    /**
     * Interface of both IPhysicsBody and ITransformable
     * @return
     */
    glm::quat getRotation() override { return getGlobalTransform().getRotation(); }
    glm::vec3 getScale() override { return getGlobalTransform().getScale(); }

    void setLocalPosition(glm::vec3 localPosition) override;

    void setLocalRotation(glm::quat localRotation) override;

    void setLocalScale(glm::vec3 localScale) override;

    void setLocalScale(float localScale) override;

    void setLocalTransform(const Transform& newLocalTransform) override;

    void setGlobalPosition(glm::vec3 globalPosition) override;

    void setGlobalRotation(glm::quat globalRotation) override;

    void setGlobalScale(glm::vec3 newScale) override;

    void setGlobalScale(float globalScale) override;

    void setGlobalTransform(const Transform& newGlobalTransform) override;

    void setGlobalTransformFromPhysics(const glm::vec3& position, const glm::quat& rotation) override;

    void translate(glm::vec3 translation) override;

    void rotate(glm::quat rotation) override;

    void rotateAxis(float angle, const glm::vec3& axis) override;

protected: // ITransformable
    Transform transform{};
    Transform cachedGlobalTransform{};
    bool bIsGlobalTransformDirty{true};

    glm::mat4 cachedModelMatrix{1.0f};

public: // IComponentContainer
    components::Component* getComponentImpl(const std::type_info& type) override
    {
        for (const auto& component : components) {
            if (typeid(*component) == type) {
                return component.get();
            }
        }
        return nullptr;
    }

    std::vector<components::Component*> getAllComponents() override
    {
        std::vector<components::Component*> _components;
        _components.reserve(components.size());
        for (auto& comp : components) {
            _components.push_back(comp.get());
        }

        return _components;
    }

    components::Component* getComponentByTypeName(std::string_view componentType) override;

    bool hasComponent(std::string_view componentType) override;

    bool canAddComponent(std::string_view componentType) override;

    components::Component* addComponent(std::unique_ptr<components::Component> component) override;

    void destroyComponent(components::Component* component) override;

    components::RigidBodyComponent* getRigidbody() const override { return rigidbodyComponent; }

    components::MeshRendererComponent* getMeshRenderer() const override { return meshRendererComponent; }

public:
    static constexpr auto TYPE = "GameObject";
    static constexpr bool CAN_BE_CREATED_MANUALLY = true;

    static std::string_view getStaticType()
    {
        return TYPE;
    }

    virtual std::string_view getComponentType() { return TYPE; }

protected: // IComponentContainer
    std::vector<std::unique_ptr<components::Component> > components{};

protected:
    components::RigidBodyComponent* rigidbodyComponent{nullptr};
    components::MeshRendererComponent* meshRendererComponent{nullptr};

public:
    bool operator==(const GameObject& other) const
    {
        return this->gameObjectId == other.gameObjectId;
    }

public: // IImguiRenderable
    void renderImgui() override {}

    void selectedRenderImgui() override;
};
}

#endif //GAME_OBJECT_H
