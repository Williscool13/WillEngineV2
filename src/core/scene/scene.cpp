//
// Created by William on 2025-01-27.
//

#include "scene.h"

#include <algorithm>
#include <cassert>

will_engine::Scene::Scene(IHierarchical* sceneRoot)
    : sceneRoot(sceneRoot)
{}

will_engine::Scene::~Scene()
{
    delete sceneRoot;

    sceneRoot = nullptr;
}

void will_engine::Scene::addGameObject(IHierarchical* gameObject)
{
    activeGameObjects.insert(gameObject);
    sceneRoot->addChild(gameObject);
}

bool will_engine::Scene::isGameObjectInScene(IHierarchical* obj)
{
    return std::ranges::find(activeGameObjects, obj) != activeGameObjects.end();
}

void will_engine::Scene::indent(IHierarchical* obj)
{
    IHierarchical* currParent = obj->getParent();
    if (currParent == nullptr) { return; }
    // get index in scene Root
    const std::vector<IHierarchical*>& currChildren = currParent->getChildren();
    int32_t index = getIndexInVector(obj, currChildren);
    if (index == -1 || index == 0) { return; }

    obj->reparent(currChildren[index - 1]);
}

void will_engine::Scene::undent(IHierarchical* obj)
{
    // find first parent under scene root
    const IHierarchical* parent = obj->getParent();
    if (parent == nullptr) { return; }

    IHierarchical* parentParent = parent->getParent();
    if (parent->getParent() == nullptr) { return; }

    // get index of in scene Root
    std::vector<IHierarchical*>& parentParentChildren = parentParent->getChildren();
    const int32_t index = getIndexInVector(parent, parentParentChildren);
    if (index == -1) { return; }

    obj->reparent(parentParent);


    // already at end, no need to reorder.
    if (index == parentParentChildren.size() - 1) { return; }
    // move the child's position to index + 1 in the children vector
    std::rotate(parentParentChildren.begin() + index + 1, parentParentChildren.end() - 1, parentParentChildren.end());
}

void will_engine::Scene::moveObject(const IHierarchical* obj, const int32_t diff)
{
    assert(diff != 0);

    std::vector<IHierarchical*>& parentChildren = obj->getParent()->getChildren();
    const int32_t index = getIndexInVector(obj, parentChildren);
    // couldn't find. big error.
    if (index == -1) { return; }

    int32_t newIndex = index + diff;
    if (newIndex < 0 || newIndex >= parentChildren.size()) { return; }

    if (index < newIndex) {
        std::rotate(parentChildren.begin() + index, parentChildren.begin() + newIndex, parentChildren.begin() + index + 2);
    } else {
        std::rotate(parentChildren.begin() + newIndex, parentChildren.begin() + index, parentChildren.begin() + index + 1);
    }
}

void will_engine::Scene::update(const int32_t currentFrameOverlap, const int32_t previousFrameOverlap) const
{
    sceneRoot->recursiveUpdate(currentFrameOverlap, previousFrameOverlap);
}

int32_t will_engine::Scene::getIndexInVector(const IHierarchical* obj, const std::vector<IHierarchical*>& vector)
{
    for (int32_t i = 0; i < vector.size(); i++) {
        if (vector[i] == obj) {
            return i;
        }
    }

    return -1;
}
