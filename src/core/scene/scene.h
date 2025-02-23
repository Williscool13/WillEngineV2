//
// Created by William on 2025-01-27.
//

#ifndef SCENE_H
#define SCENE_H

#include <unordered_set>
#include <vulkan/vulkan_core.h>

#include "src/core/game_object/hierarchical.h"


namespace will_engine
{
class Scene
{
public:
    Scene() = delete;

    Scene(IHierarchical* sceneRoot);

    ~Scene();

    void addGameObject(IHierarchical* gameObject);

    bool isGameObjectInScene(IHierarchical* obj);

    static void indent(IHierarchical* obj);

    static void undent(IHierarchical* obj);

    static void moveObject(const IHierarchical* obj, int diff);

private: // Scene properties
    IHierarchical* sceneRoot;
    std::unordered_set<IHierarchical*> activeGameObjects;

public:
    void update(float deltaTime);

    static void recursiveUpdate(IHierarchical* object, float deltaTime);

    IHierarchical* getRoot() const { return sceneRoot; }

private:
    /**
     * Gets the index of the specified gameobject in the vector. Returns -1 if not found. O(n) complexity
     * @param obj
     * @param vector
     * @return
     */
    static int getIndexInVector(const IHierarchical* obj, const std::vector<IHierarchical*>& vector);
};
}


#endif //SCENE_H
