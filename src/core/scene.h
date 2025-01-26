//
// Created by William on 2025-01-27.
//

#ifndef SCENE_H
#define SCENE_H
#include <unordered_set>

#include "game_object/game_object.h"

namespace will_engine
{
class Scene
{
public:
    Scene();

    ~Scene();

    void addGameObject(GameObject* gameObject);

    bool isGameObjectInScene(GameObject* obj);

    static void parentGameObject(GameObject* obj);

    static void unparentGameObject(GameObject* obj);

    void moveObject(const GameObject* obj, int diff) const;

private: // Scene properties
    GameObject* sceneRoot;
    std::unordered_set<GameObject*> activeGameObjects;


private: // Dear ImGui


public:
    void imguiSceneGraph();

    void displayGameObject(GameObject* obj, int32_t depth = 0);

    void update() const;

private:
    /**
     * Gets the index of the specified gameobject in the vector. Returns -1 if not found. O(n) complexity
     * @param obj
     * @param vector
     * @return
     */
    static int getIndexInVector(const GameObject* obj, const std::vector<GameObject*>& vector);

    /**
     * Deletes the specified gameobjects and all its children recursively.
     * @param obj
     */
    void deleteGameObjectRecursive(GameObject* obj);
};
}


#endif //SCENE_H
