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

namespace will_engine::game
{
class RigidBodyComponent;
class MeshRendererComponent;

class GameObject : public ITransformable,
                   public IHierarchical,
                   public IImguiRenderable,
                   public IComponentContainer
{
public:
    explicit GameObject(std::string gameObjectName = "");

    ~GameObject() override;

    void destroy() override;

    void recursivelyDestroy() override;

    void recursivelyUpdate(float deltaTime) override;

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

    void beginDestructor() override;

    IHierarchical* addChild(std::unique_ptr<IHierarchical> child, bool retainTransform = false) override;

    bool moveChild(IHierarchical* child, IHierarchical* newParent, bool retainTransform = true) override;

    void moveChildToIndex(int32_t fromIndex, int32_t targetIndex) override;

    bool deleteChild(IHierarchical* child) override;;

    void dirty() override;

    void setParent(IHierarchical* newParent) override;

    IHierarchical* getParent() const override;

    const std::vector<IHierarchical*>& getChildren() override;

    void setName(std::string newName) override;

    std::string_view getName() const override { return gameObjectName; }

protected: // IHierarchical
    IHierarchical* parent{nullptr};
    /**
     * Cache parent's transformable interface to reduce excessive casting
     */
    ITransformable* transformableParent{nullptr};
    std::vector<std::unique_ptr<IHierarchical>> children{};

    std::vector<IHierarchical*> childrenCache{};
    bool bChildrenCacheDirty{false};

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
    Component* getComponentImpl(const std::type_info& type) override
    {
        for (const auto& component : components) {
            if (typeid(*component) == type) {
                return component.get();
            }
        }
        return nullptr;
    }

    std::vector<Component*> getComponentsImpl(const std::type_info& type) override
    {
        std::vector<Component*> outComponents;
        outComponents.reserve(components.size());
        for (const auto& component : components) {
            if (typeid(*component) == type) {
                outComponents.push_back(component.get());
            }
        }

        return outComponents;
    }

    std::vector<Component*> getAllComponents() override
    {
        std::vector<Component*> _components;
        _components.reserve(components.size());
        for (auto& comp : components) {
            _components.push_back(comp.get());
        }

        return _components;
    }

    /**
     * Returns the first component of the type specified.
     * @param componentType
     * @return
     */
    Component* getComponentByTypeName(std::string_view componentType) override;

    /**
     * Returns all components of the type specified.
     * @param componentType
     * @return
     */
    std::vector<Component*> getComponentsByTypeName(std::string_view componentType) override;

    bool hasComponent(std::string_view componentType) override;

    /**
     *
     * @param component
     * @return returns a pointer to the newly created component
     */
    Component* addComponent(std::unique_ptr<Component> component) override;

    void destroyComponent(Component* component) override;

    std::vector<MeshRendererComponent*> getMeshRendererComponents();

public:
    static constexpr auto TYPE = "GameObject";
    static constexpr bool CAN_BE_CREATED_MANUALLY = true;

    static std::string_view getStaticType()
    {
        return TYPE;
    }

    virtual std::string_view getComponentType() { return TYPE; }

protected: // IComponentContainer
    std::vector<std::unique_ptr<Component> > components{};

protected:
    RigidBodyComponent* rigidbodyComponent{nullptr};
    std::vector<MeshRendererComponent*> cachedMeshRendererComponents{};
    bool bIsCachedMeshRenderersDirty{false};

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
