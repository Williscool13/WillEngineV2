//
// Created by William on 2024-08-24.
//

#include "scene.h"

#include <imgui.h>

#include <utility>

Scene::Scene()
{
    sceneRoot = new GameObject("Scene Root");
}

Scene::~Scene()
{
    deleteGameObjectRecursive(sceneRoot);

    sceneRoot = nullptr;
}

GameObject* Scene::createGameObject(std::string name)
{
    auto* newGameObject = new GameObject(std::move(name));
    activeGameObjects.insert(newGameObject);
    sceneRoot->addChild(newGameObject);
    return newGameObject;
}

bool Scene::isGameObjectValid(GameObject* obj)
{
    return std::ranges::find(activeGameObjects, obj) != activeGameObjects.end();
}

void Scene::displayGameObject(GameObject* obj, int depth)
{
    constexpr float indent = 10.0f;
    constexpr float treeNodeWidth = 200.0f;
    constexpr float buttonWidth = 75.0f;
    constexpr float arrowWidth = 20.0f;
    constexpr float spacing = 5.0f;

    ImGui::PushID(obj);
    ImGui::Indent(depth * indent);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (obj->getChildren().empty()) { flags |= ImGuiTreeNodeFlags_Leaf; }

    ImGui::SetNextItemWidth(treeNodeWidth);
    bool isOpen = ImGui::TreeNodeEx("##TreeNode", flags, "%s", obj->getName().data());
    //bool isOpen = ImGui::TreeNodeEx((void *) (intptr_t) obj->getId(), flags, obj->getName().data());

    ImGui::SameLine(treeNodeWidth + spacing);
    ImGui::BeginGroup();

    GameObject* parent = obj->getParent();
    if (parent != nullptr) {
        std::vector<GameObject*>& parentChildren = obj->getParent()->getChildren();

        if (parent != sceneRoot) {
            if (ImGui::Button("Unparent", ImVec2(buttonWidth, 0))) { unparentGameObject(obj); }
        } else { ImGui::Dummy(ImVec2(buttonWidth, 0)); }

        ImGui::SameLine(0, spacing);
        if (parentChildren[0] != obj) {
            if (ImGui::Button("Parent", ImVec2(buttonWidth, 0))) { parentGameObject(obj); }
        } else { ImGui::Dummy(ImVec2(buttonWidth, 0)); }

        ImGui::SameLine(0, spacing);
        if (parentChildren[0] != obj) {
            if (ImGui::ArrowButton("##Up", ImGuiDir_Up)) { moveObject(obj, -1); }
        } else { ImGui::Dummy(ImVec2(arrowWidth, 0)); }

        ImGui::SameLine(0, spacing);
        if (parentChildren[parentChildren.size() - 1] != obj) {
            if (ImGui::ArrowButton("##Down", ImGuiDir_Down)) { moveObject(obj, 1); }
        } else { ImGui::Dummy(ImVec2(arrowWidth, 0)); }
    }

    ImGui::EndGroup();

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

    ImGui::Unindent(depth * indent);
    ImGui::PopID();
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

void Scene::deleteGameObjectRecursive(GameObject* obj)
{
    if (obj == nullptr) { return; }
    // Reverse iterate
    std::vector<GameObject*>& children = obj->getChildren();
    for (int i = children.size() - 1; i >= 0; i--) {
        deleteGameObjectRecursive(children[i]);
    }

    activeGameObjects.erase(obj);
    delete obj;
}
