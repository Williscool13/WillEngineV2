//
// Created by William on 2025-01-27.
//

#include "scene.h"

#include <imgui.h>

will_engine::Scene::Scene()
{
    sceneRoot = new GameObject("Scene Root");
}

will_engine::Scene::~Scene()
{
    deleteGameObjectRecursive(sceneRoot);

    sceneRoot = nullptr;
}

void will_engine::Scene::addGameObject(GameObject* gameObject)
{
    activeGameObjects.insert(gameObject);
    sceneRoot->addChild(gameObject);
}

bool will_engine::Scene::isGameObjectInScene(GameObject* obj)
{
    return std::ranges::find(activeGameObjects, obj) != activeGameObjects.end();
}

void will_engine::Scene::parentGameObject(GameObject* obj)
{
    GameObject* currParent = obj->getParent();
    if (currParent == nullptr) { return; }
    // get index in scene Root
    const std::vector<GameObject*>& currChildren = currParent->getChildren();
    int32_t index = getIndexInVector(obj, currChildren);
    if (index == -1 || index == 0) { return; }

    currChildren[index - 1]->addChild(obj, true);
}

void will_engine::Scene::unparentGameObject(GameObject* obj)
{
    // find first parent under scene root
    GameObject* parent = obj->getParent();
    if (parent == nullptr) { return; }

    GameObject* parentParent = parent->getParent();
    if (parent->getParent() == nullptr) { return; }

    // get index of in scene Root
    std::vector<GameObject*>& parentParentChildren = parentParent->getChildren();
    const int32_t index = getIndexInVector(parent, parentParentChildren);
    if (index == -1) { return; }

    parentParent->addChild(obj);


    // already at end, no need to reorder.
    if (index == parentParentChildren.size() - 1) { return; }
    // move the child's position to index + 1 in the children vector
    std::rotate(parentParentChildren.begin() + index + 1, parentParentChildren.end() - 1, parentParentChildren.end());
}

void will_engine::Scene::moveObject(const GameObject* obj, const int32_t diff) const
{
    if (obj == sceneRoot || obj->getParent() == nullptr) { return; }
    assert(diff != 0);

    std::vector<GameObject*>& parentChildren = obj->getParent()->getChildren();
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

void will_engine::Scene::imguiSceneGraph()
{
    if (ImGui::Begin("Scene Graph")) {
        if (sceneRoot != nullptr && !sceneRoot->getChildren().empty()) {
            for (GameObject* child : sceneRoot->getChildren()) {
                displayGameObject(child, 0);
            }
        } else {
            ImGui::Text("Scene is empty");
        }
    }
    ImGui::End();
}

void will_engine::Scene::displayGameObject(GameObject* obj, const int32_t depth) // NOLINT(*-no-recursion)
{
    constexpr int32_t indent = 10.0f;
    constexpr float treeNodeWidth = 200.0f;
    constexpr float spacing = 5.0f;

    ImGui::PushID(obj);
    ImGui::Indent(static_cast<float
        >(depth * indent));

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (obj->getChildren().empty()) { flags |= ImGuiTreeNodeFlags_Leaf; }

    ImGui::SetNextItemWidth(treeNodeWidth);
    bool isOpen = ImGui::TreeNodeEx("##TreeNode", flags, "%s", obj->getName().data());
    //bool isOpen = ImGui::TreeNodeEx((void *) (intptr_t) obj->getId(), flags, obj->getName().data());

    ImGui::SameLine(treeNodeWidth + spacing);
    ImGui::BeginGroup();

    const GameObject* parent = obj->getParent();
    if (parent != nullptr) {
        constexpr float arrowWidth = 20.0f;
        constexpr float buttonWidth = 75.0f;
        const std::vector<GameObject*>& parentChildren = obj->getParent()->getChildren();

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

    if (isOpen) {
        for (GameObject* child : obj->getChildren()) {
            displayGameObject(child, depth + 1);
        }
        ImGui::TreePop();
    }

    ImGui::Unindent(static_cast<float>(depth * indent));
    ImGui::PopID();
}

void will_engine::Scene::update(const int32_t currentFrameOverlap, const int32_t previousFrameOverlap) const
{
    sceneRoot->recursiveUpdateModelMatrix(currentFrameOverlap, previousFrameOverlap);
}

int32_t will_engine::Scene::getIndexInVector(const GameObject* obj, const std::vector<GameObject*>& vector)
{
    for (int32_t i = 0; i < vector.size(); i++) {
        if (vector[i] == obj) {
            return i;
        }
    }

    return -1;
}

void will_engine::Scene::deleteGameObjectRecursive(GameObject* obj)  // NOLINT(*-no-recursion)
{
    if (obj == nullptr) { return; }
    // Reverse iterate
    std::vector<GameObject*>& children = obj->getChildren();
    for (int32_t i = static_cast<int32_t>(children.size()) - 1; i >= 0; i--) {
        deleteGameObjectRecursive(children[i]);
    }

    activeGameObjects.erase(obj);
    delete obj;
}
