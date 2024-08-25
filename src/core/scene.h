//
// Created by William on 2024-08-24.
//

#ifndef SCENE_H
#define SCENE_H
#include "game_object.h"


class Scene {
public:
    Scene();
    ~Scene();
private: // Scene properties
    GameObject* sceneRoot;

    void displayGameObject(GameObject* obj, int depth = 0);

    void parentGameObject(GameObject* obj);

    void unparentGameObject(GameObject* obj);

    void moveObject(GameObject* obj, int diff);

public:
    void addToRoot(GameObject* gameObject);

    void imguiSceneGraph();

private:
    int getIndexInVector(GameObject* obj, std::vector<GameObject*> vector);
};



#endif //SCENE_H
