//
// Created by William on 2024-08-24.
//

#ifndef SCENE_H
#define SCENE_H
#include <unordered_set>

#include "game_object.h"


class Scene
{
public:
    Scene();

    ~Scene();

    GameObject* createGameObject(std::string name);

    void addGameObject(GameObject* gameObject);

    bool isGameObjectValid(GameObject* obj);

private: // Scene properties
    GameObject* sceneRoot;
    std::unordered_set<GameObject*> activeGameObjects;

    void displayGameObject(GameObject* obj, int depth = 0);

    void parentGameObject(GameObject* obj);

    void unparentGameObject(GameObject* obj);

    void moveObject(GameObject* obj, int diff);

public:
    void imguiSceneGraph();

    void updateSceneModelMatrices() const;

private:
    static int getIndexInVector(GameObject* obj, std::vector<GameObject*> vector);

    void deleteGameObjectRecursive(GameObject* obj);

public: // Debug
    GameObject* DEBUG_getSceneRoot() const;
};


#endif //SCENE_H
