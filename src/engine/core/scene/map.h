//
// Created by William on 2025-03-08.
//

#ifndef MAP_H
#define MAP_H
#include <filesystem>
#include <json/json.hpp>

#include "engine/core/game_object/component_container.h"
#include "engine/core/game_object/hierarchical.h"
#include "engine/core/game_object/transformable.h"
#include "engine/core/game_object/components/terrain_component.h"
#include "engine/renderer/assets/asset_manager.h"
#include "engine/util/heightmap_utils.h"

namespace will_engine::game
{
/**
 * Maps represent the top-most object in a scene hierarchy. It has no parents and can have an unbound number of children.
 */
class Map final : public IHierarchical,
                  public ITransformable,
                  public IComponentContainer
{
public:
    explicit Map(const std::filesystem::path& mapSource, renderer::ResourceManager& resourceManager);

    ~Map() override;

    /**
     * Destroys the map and all gameobjects that are children of this map
     */
    void destroy() override;

    bool loadMap();

    bool saveMap();

    bool saveMap(const std::filesystem::path& newSavePath);

    int32_t getMapId() const { return mapId; }

    std::filesystem::path getMapPath() const { return mapSource; }

#pragma region Interfaces

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

    Component* getComponentByTypeName(std::string_view componentType) override;

    std::vector<Component*> getComponentsByTypeName(std::string_view componentType) override;

    bool hasComponent(std::string_view componentType) override;

    Component* addComponent(std::unique_ptr<Component> component) override;

    void destroyComponent(Component* component) override;

protected: // IComponentContainer
    std::vector<std::unique_ptr<Component> > components{};

protected:
    TerrainComponent* terrainComponent{nullptr};

public: // IHierarchical
    void beginPlay() override;

    /**
     * Updates all gameobjects under this map
     * @param deltaTime
     */
    void update(float deltaTime) override;

    void beginDestructor() override;

    IHierarchical* addChild(std::unique_ptr<IHierarchical> child, bool retainTransform = false) override;

    bool moveChild(IHierarchical* child, IHierarchical* newParent, bool retainTransform = true) override;

    void moveChildToIndex(int32_t fromIndex, int32_t targetIndex) override;

    bool deleteChild(IHierarchical* child) override;;

    void recursivelyUpdate(float deltaTime) override;

    void recursivelyDestroy() override;


    void dirty() override;

    void setParent(IHierarchical* newParent) override;

    IHierarchical* getParent() const override;

    std::vector<IHierarchical*>& getChildren() override;

    void setName(std::string newName) override;

    std::string_view getName() const override { return mapName; }

private: // IHierarchical
    std::vector<std::unique_ptr<IHierarchical>> children{};

    std::vector<IHierarchical*> childrenCache{};
    bool bChildrenCacheDirty{false};


    bool bHasBegunPlay{false};

public: // ITransformable
    glm::mat4 getModelMatrix() override { return cachedModelMatrix; }

    const Transform& getLocalTransform() const override { return transform; }
    glm::vec3 getLocalPosition() const override { return getLocalTransform().getPosition(); }
    glm::quat getLocalRotation() const override { return getLocalTransform().getRotation(); }
    glm::vec3 getLocalScale() const override { return getLocalTransform().getScale(); }

    const Transform& getGlobalTransform() override { return transform; }

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

    void setGlobalPosition(const glm::vec3 globalPosition) override { setLocalPosition(globalPosition); }

    void setGlobalRotation(const glm::quat globalRotation) override { setLocalRotation(globalRotation); }

    void setGlobalScale(const glm::vec3 newScale) override { setLocalScale(newScale); }

    void setGlobalScale(const float globalScale) override { setGlobalScale(glm::vec3(globalScale)); }

    void setGlobalTransform(const Transform& newGlobalTransform) override { setLocalTransform(newGlobalTransform); }

    void setGlobalTransformFromPhysics(const glm::vec3& position, const glm::quat& rotation) override {}

    void translate(glm::vec3 translation) override;

    void rotate(glm::quat rotation) override;

    void rotateAxis(float angle, const glm::vec3& axis) override;

private: // ITransformable
    Transform transform{};
    glm::mat4 cachedModelMatrix{1.0f};

#pragma endregion

private:
    std::filesystem::path mapSource{};
    std::string mapName{};
    uint32_t mapId;

    renderer::ResourceManager& resourceManager;

    bool isLoaded{false};
};
}


#endif //MAP_H
