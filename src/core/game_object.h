//
// Created by William on 2024-08-24.
//

#ifndef GAME_OBJECT_H
#define GAME_OBJECT_H

#include <string>

#include <glm/glm.hpp>
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

    ~GameObject();

public: // Hierarchy
    /**
     * Adds a \code GameObject\endcode as a child to this \code GameObject\endcode while maintaining its position in WorldSpace.
     * \n Will reparent the child if that child has a parent.
     * @param child
     * @param maintainWorldPosition
     */
    void addChild(GameObject* child, bool maintainWorldPosition = true);

    /**
     * If exists, removes a child from the children array and unparents the child.
     * @param child
     */
    void removeChild(GameObject* child);

    /**
     * Unparents a child while maintaining its position in WorldSpace
     */
    void unparent();


    GameObject* getParent();

    std::vector<GameObject*>& getChildren();

    int getId() const { return gameObjectId; }

    std::string_view getName() const { return gameObjectName; }

private: // Hierarchy
    GameObject* parent{nullptr};
    std::vector<GameObject*> children{};
    static int nextId;
    int gameObjectId{};
    std::string gameObjectName{};

public: // Transform
    Transform transform{};

    /**
    * Should be called whenever transform is modified
    */
    void dirty();

private: // Transform
    glm::mat4 cachedWorldTransform{};
    bool bIsTransformDirty{true};
    bool bModelPendingUpdate{true};

    glm::mat4 getModelMatrix();

public: // Rendering
    /**
     * Update the model transform in the RenderObject. Only applicable if this gameobject has a pRenderObject associated with it.
     */
    void recursiveUpdateModelMatrix();

private: // Rendering
    /**
      * If true, the model matrix will never be updated from defaults.
      */
    bool bIsStatic{false};
    /**
     * If true, the model matrix in the mapped CPU buffer of the RenderObject will be updated in the following frame
     */
    bool bModelUpdatedLastFrame{true};
    /**
     * The render object that is responsible for drawing this gameobject's model
     */
    RenderObject* pRenderObject{nullptr};
    int32_t instanceIndex{-1};

public: // Rendering
    void setRenderObjectReference(RenderObject* owner, int32_t index)
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
