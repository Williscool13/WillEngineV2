//
// Created by William on 2024-08-24.
//

#ifndef GAME_OBJECT_H
#define GAME_OBJECT_H
#include <string>

#include "../util/transform.h"
#include <vulkan/vulkan_core.h>

class RenderObject;

//using TransformChangedCallback = std::function<void(const glm::mat4&)>;

class GameObject
{
public:
    GameObject();

    explicit GameObject(std::string gameObjectName);

    ~GameObject();

private:
    /*std::vector<TransformChangedCallback> transformCallbacks;
    void addTransformCallback(TransformChangedCallback callback) {
        transformCallbacks.push_back(std::move(callback));
    }

    /**
     * O(n) callback removal
     * @param callback
     #1#
    void removeTransformCallback(const TransformChangedCallback& callback)
    {
        auto it = std::find_if(transformCallbacks.begin(), transformCallbacks.end(),
            [&callback](const TransformChangedCallback& cb) {
                return cb.target_type() == callback.target_type();
            });
        if (it != transformCallbacks.end()) {
            transformCallbacks.erase(it);
        }
    }*/

public: // Hierarchy
    /**
     * Adds a \code GameObject\endcode as a child to this \code GameObject\endcode while maintaining its position in WorldSpace.
     * \n Will reparent the child if that child has a parent.
     * @param child
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

    std::vector<GameObject*> &getChildren();

    int getId() const { return gameObjectId; }

    std::string_view getName() const { return gameObjectName; }

private: // Transform
    glm::mat4 cachedWorldTransform{};
    bool isTransformDirty{true};

public:
    /**
     * If modified, after finish all modifications, call \code refreshTransform\endcode
     */
    Transform transform{};

    /**
     * Should be called whenever transform is modified
     */
    void refreshTransforms();

    glm::mat4 getModelMatrix();

private: // Hierarchy
    GameObject* parent{nullptr};
    std::vector<GameObject*> children{};
    static int nextId;
    int gameObjectId{};
    std::string gameObjectName{};

public: // Rendering
    void setRenderObjectReference(RenderObject* owner, int32_t index)
    {
        instanceOwner = owner;
        instanceIndex = index;
    }

private: // Rendering
    RenderObject* instanceOwner{nullptr};
    int32_t instanceIndex{-1};

public:
    bool operator==(const GameObject& other) const
    {
        return this->gameObjectId == other.gameObjectId;
    }
};


#endif //GAME_OBJECT_H
