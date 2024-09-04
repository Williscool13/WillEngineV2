//
// Created by William on 2024-08-24.
//

#ifndef SCENE_H
#define SCENE_H
#include <unordered_set>

#include "game_object.h"


class Scene {
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

private:
    int getIndexInVector(GameObject* obj, std::vector<GameObject*> vector);

private:
    void deleteGameObjectRecursive(GameObject* obj);
};



#endif //SCENE_H
