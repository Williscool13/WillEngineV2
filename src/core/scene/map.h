//
// Created by William on 2025-03-08.
//

#ifndef MAP_H
#define MAP_H
#include <filesystem>

#include "src/core/game_object/hierarchical.h"
#include "src/core/game_object/transformable.h"

namespace will_engine
{
/**
 * Maps do no have parents, they are the roots of their respctive hierarchies
 */
class Map final : public IHierarchical, public ITransformable
{
public:
    explicit Map(const std::filesystem::path& mapSource);

    ~Map() override;

    void destroy() override;



    bool loadMap();

    bool saveMap(const std::filesystem::path& newSavePath = {});

    int32_t getMapId() const { return mapId; }

    void addGameObject(IHierarchical* newChild);

public: // IHierarchical
    void beginPlay() override;

    /**
     * Updates all gameobjects under this map
     * @param deltaTime
     */
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

    std::string_view getName() const override { return mapName; }

private: // IHierarchical
    std::vector<IHierarchical*> children{};

    void recursiveUpdate(IHierarchical* object, float deltaTime);

    void recursiveDestroy(IHierarchical* object);

public: // ITransformable
    glm::mat4 getModelMatrix() override { return getGlobalTransform().getTRSMatrix(); }

    const Transform& getLocalTransform() const override { return transform; }
    glm::vec3 getLocalPosition() const override { return getLocalTransform().getPosition(); }
    glm::quat getLocalRotation() const override { return getLocalTransform().getRotation(); }
    glm::vec3 getLocalScale() const override { return getLocalTransform().getScale(); }

    const Transform& getGlobalTransform() override { return transform; }

    /**
     * Interface of both IPhysicsBody and ITransformable
     * @return
     */
    glm::vec3 getGlobalPosition() override { return getGlobalTransform().getPosition(); }
    /**
     * Interface of both IPhysicsBody and ITransformable
     * @return
     */
    glm::quat getGlobalRotation() override { return getGlobalTransform().getRotation(); }
    glm::vec3 getGlobalScale() override { return getGlobalTransform().getScale(); }

    void setLocalPosition(glm::vec3 localPosition) override;

    void setLocalRotation(glm::quat localRotation) override;

    void setLocalScale(glm::vec3 localScale) override;

    void setLocalScale(float localScale) override;

    void setLocalTransform(const Transform& newLocalTransform) override;

    void setGlobalPosition(glm::vec3 globalPosition) override { setLocalPosition(globalPosition); }

    void setGlobalRotation(glm::quat globalRotation) override { setLocalRotation(globalRotation); }

    void setGlobalScale(glm::vec3 newScale) override { setLocalScale(newScale); }

    void setGlobalScale(float globalScale) override { setGlobalScale(glm::vec3(globalScale)); }

    void setGlobalTransform(const Transform& newGlobalTransform) override { setLocalTransform(newGlobalTransform); }

    void setGlobalTransformFromPhysics(const glm::vec3& position, const glm::quat& rotation) override {}

    void translate(glm::vec3 translation) override;

    void rotate(glm::quat rotation) override;

    void rotateAxis(float angle, const glm::vec3& axis) override;

private: // ITransformable
    Transform transform{};
    Transform cachedGlobalTransform{};
    bool bIsGlobalTransformDirty{true};

private:
    std::filesystem::path mapSource{};
    std::string mapName{};
    uint32_t mapId;

    bool canBeLoaded{false};
    bool isLoaded{false};
};
}


#endif //MAP_H
