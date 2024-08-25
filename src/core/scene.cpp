//
// Created by William on 2024-08-24.
//

#include "scene.h"

#include <imgui.h>

Scene::Scene()
{
    sceneRoot = new GameObject("Scene Root");
}

Scene::~Scene()
{
    // destroy children depth first search
}

void Scene::displayGameObject(GameObject* obj, int depth)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    if (obj->getChildren().empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    bool isOpen = ImGui::TreeNodeEx((void *) (intptr_t) obj->getId(), flags, obj->getName().data());

    const ImVec2 parentButtonSize = {75,20};
    const ImVec2 arrowButtonSize  = {20,20};
    const float buttonSpacing = ImGui::GetStyle().ItemSpacing.x;

    auto* parent = obj->getParent();
    if (parent != nullptr) {
        std::vector<GameObject *>& parentChildren = obj->getParent()->getChildren();

        ImGui::SameLine();
        if (obj->getParent() != sceneRoot) {
            if (ImGui::Button("Unparent##Unparent", parentButtonSize)) { unparentGameObject(obj); }
        } else {
            ImGui::Dummy(parentButtonSize);
        }

        ImGui::SameLine();
        if (parentChildren[0] != obj) {
            if (ImGui::Button("Parent##Parent", parentButtonSize)) { parentGameObject(obj); }
        } else {
            ImGui::Dummy(parentButtonSize);
        }

        ImGui::SameLine();
        if (parentChildren[0] != obj) {
            if (ImGui::Button("^##MoveUp", arrowButtonSize)) { moveObject(obj, -1); }
        } else {
            ImGui::Dummy(arrowButtonSize);
        }

        ImGui::SameLine();
        if (parentChildren[parentChildren.size() - 1] != obj) {
            if (ImGui::Button("v##MoveDown", arrowButtonSize)) { moveObject(obj, 1); }
        } else {
            ImGui::Dummy(arrowButtonSize);
        }
    }



    /*if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("GAMEOBJECT_DRAG", &obj, sizeof(GameObject *));
        ImGui::Text("Moving %s", obj->getName().data());
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GAMEOBJECT_DRAG")) {
            GameObject* droppedObj = *static_cast<GameObject **>(payload->Data);
            obj->addChild(droppedObj);
        }
        ImGui::EndDragDropTarget();
    }*/

    //if (ImGui::IsItemClicked()) {
    // Handle click
    //}

    if (isOpen) {
        for (auto* child: obj->getChildren()) {
            displayGameObject(child, depth + 1);
        }
        ImGui::TreePop();
    }
}

void Scene::parentGameObject(GameObject* obj)
{
    GameObject* currParent = obj->getParent();
    if (currParent == nullptr) { return; }
    // get index in scene Root
    int index{-1};
    std::vector<GameObject *>& currChildren = currParent->getChildren();
    index = getIndexInVector(obj, currChildren);
    if (index == -1 || index == 0) { return; }

    currChildren[index - 1]->addChild(obj);
}

void Scene::unparentGameObject(GameObject* obj)
{
    // find first parent under scene root
    GameObject* parent = obj->getParent();
    if (parent == nullptr) { return; }

    GameObject* parentParent = parent->getParent();
    if (parent->getParent() == nullptr) { return; }

    // get index of in scene Root
    int index{-1};
    std::vector<GameObject *>& parentParentChildren = parentParent->getChildren();
    index = getIndexInVector(parent, parentParentChildren);
    if (index == -1) { return; }

    parentParent->addChild(obj);


    // already at end, no need to reorder.
    if (index == parentParentChildren.size() - 1) { return; }
    // move the child's position to index + 1 in the children vector
    std::rotate(parentParentChildren.begin() + index + 1, parentParentChildren.end() - 1, parentParentChildren.end());
}

void Scene::moveObject(GameObject* obj, int diff)
{
    if (obj == sceneRoot || obj->getParent() == nullptr) { return; }
    assert(diff != 0);

    int index{-1};
    std::vector<GameObject *>& parentChildren = obj->getParent()->getChildren();
    index = getIndexInVector(obj, parentChildren);
    // couldn't find. big error.
    if (index == -1) { return; }

    int newIndex = index + diff;
    if (newIndex < 0 || newIndex >= parentChildren.size()) { return; }

    if (index < newIndex) {
        std::rotate(parentChildren.begin() + index, parentChildren.begin() + newIndex, parentChildren.begin() + index + 2);
    } else {
        std::rotate(parentChildren.begin() + newIndex, parentChildren.begin() + index, parentChildren.begin() + index + 1);
    }
}

void Scene::addToRoot(GameObject* gameObject)
{
    sceneRoot->addChild(gameObject);
}

void Scene::imguiSceneGraph()
{
    ImGui::Begin("Scene Graph");

    if (sceneRoot != nullptr && !sceneRoot->getChildren().empty()) {
        for (auto* child: sceneRoot->getChildren()) {
            displayGameObject(child, 0);
        }
    } else {
        ImGui::Text("Scene is empty");
    }

    ImGui::End();
}

int Scene::getIndexInVector(GameObject* obj, std::vector<GameObject *> vector)
{
    for (int i = 0; i < vector.size(); i++) {
        if (vector[i] == obj) {
            return i;
            break;
        }
    }

    return -1;
}
