//
// Created by William on 2024-08-24.
//

#ifndef GAME_OBJECT_H
#define GAME_OBJECT_H

#include <string>

#include <glm/glm.hpp>
#include <src/renderer/renderer_constants.h>
#include <vulkan/vulkan_core.h>

#include "src/physics/physics.h"
#include "src/util/transform.h"

class RenderObject;

//using TransformChangedCallback = std::function<void(const glm::mat4&)>;

class GameObject
{
public:
    GameObject();

    explicit GameObject(std::string gameObjectName);

    virtual ~GameObject();

    virtual void update(float deltaTime) {}

    void setName(std::string newName);
public: // Hierarchy
    /**
     * Adds a \code GameObject\endcode as a child to this \code GameObject\endcode while maintaining its position in WorldSpace.
     * \n Will reparent the child if that child has a parent.
     * @param child
     * @param maintainWorldTransform
     */
    void addChild(GameObject* child, bool maintainWorldTransform = true);

    /**
     * If exists, removes a child from the children array and unparents the child.
     */
    void removeChild(GameObject* child, GameObject* newParent = nullptr);

    /**
     * Unparents a child while maintaining its position in WorldSpace
     */
    void reparent(GameObject* newParent = nullptr, bool maintainWorldTransform = true);


    GameObject* getParent();

    std::vector<GameObject*>& getChildren();

    uint32_t getId() const { return gameObjectId; }

    std::string_view getName() const { return gameObjectName; }

protected: // Hierarchy
    GameObject* parent{nullptr};
    std::vector<GameObject*> children{};
    static uint32_t nextId;
    uint32_t gameObjectId{};
    std::string gameObjectName{};

private: // Transform
    Transform transform{};
    Transform cachedGlobalTransform{};
    bool bIsGlobalTransformDirty{true};
    /**
     * Will update the model matrix on GPU if this number is greater than 0
     */
    int32_t framesToUpdate{FRAME_OVERLAP + 1};

    void dirty();

public: // Transform
    glm::mat4 getModelMatrix() { return getGlobalTransform().getTRSMatrix(); }

    const Transform& getLocalTransform() const { return transform; }
    glm::vec3 getLocalPosition() const { return getLocalTransform().getPosition(); }
    glm::quat getLocalRotation() const { return getLocalTransform().getRotation(); }
    glm::vec3 getLocalScale() const { return getLocalTransform().getScale(); }

    const Transform& getGlobalTransform();
    glm::vec3 getGlobalPosition() { return getGlobalTransform().getPosition(); }
    glm::quat getGlobalRotation() { return getGlobalTransform().getRotation(); }
    glm::vec3 getGlobalScale()    { return getGlobalTransform().getScale(); }

    void setLocalPosition(glm::vec3 localPosition);
    void setLocalRotation(glm::quat localRotation);
    void setLocalScale(glm::vec3 localScale);
    void setLocalScale(const float localScale) { setLocalScale(glm::vec3(localScale)); }
    void setLocalTransform(const Transform& newLocalTransform);

    void setGlobalPosition(glm::vec3 globalPosition);
    void setGlobalRotation(glm::quat globalRotation);
    void setGlobalScale(glm::vec3 globalScale);
    void setGlobalScale(const float globalScale) { setGlobalScale(glm::vec3(globalScale)); }
    void setGlobalTransform(const Transform& newGlobalTransform);

    void translate(const glm::vec3 translation)
    {
        transform.translate(translation);
        dirty();
    }

    void rotate(const glm::quat rotation)
    {
        transform.rotate(rotation);
        dirty();
    }

    void rotateAxis(const float angle, const glm::vec3& axis)
    {
        transform.rotateAxis(angle, axis);
        dirty();
    }

public: // Rendering
    /**
     * Update the model transform in the RenderObject. Only applicable if this gameobject has a pRenderObject associated with it.
     */
    void recursiveUpdateModelMatrix(int32_t previousFrameOverlapIndex, int32_t currentFrameOverlapIndex);

private: // Rendering
    /**
      * If true, the model matrix will never be updated from defaults.
      */
    bool bIsStatic{false};
    /**
     * The render object that is responsible for drawing this gameobject's model
     */
    RenderObject* pRenderObject{nullptr};
    int32_t instanceIndex{-1};

public: // Rendering
    void setRenderObjectReference(RenderObject* owner, const int32_t index)
    {
        pRenderObject = owner;
        instanceIndex = index;
    }

private: // Physics
    JPH::BodyID bodyID;

public:
    bool operator==(const GameObject& other) const
    {
        return this->gameObjectId == other.gameObjectId;
    }
};


#endif //GAME_OBJECT_H
