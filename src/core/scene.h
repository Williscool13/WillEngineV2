//
// Created by William on 2025-01-27.
//

#ifndef SCENE_H
#define SCENE_H

#include <unordered_set>
#include <vulkan/vulkan_core.h>

#include "game_object/hierarchical.h"

namespace will_engine
{
class Scene
{
public:
    Scene();

    ~Scene();

    void addGameObject(IHierarchical* gameObject);

    bool isGameObjectInScene(IHierarchical* obj);

    static void indent(IHierarchical* obj);

    static void undent(IHierarchical* obj);

    void moveObject(const IHierarchical* obj, int diff) const;

private: // Scene properties
    IHierarchical* sceneRoot;
    std::unordered_set<IHierarchical*> activeGameObjects;

private: // Dear ImGui


public:
    void imguiSceneGraph();

    void displayGameObject(IHierarchical* obj, int32_t depth = 0);

    void update(int32_t currentFrameOverlap, int32_t previousFrameOverlap) const;

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
