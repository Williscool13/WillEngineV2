//
// Created by William on 2025-01-27.
//

#include "scene.h"

#include <algorithm>
#include <imgui.h>

#include "src/core/game_object/game_object.h"


will_engine::Scene::Scene()
{
    sceneRoot = new GameObject("Scene Root");
}

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

void will_engine::Scene::moveObject(const IHierarchical* obj, const int32_t diff) const
{
    if (obj == sceneRoot || obj->getParent() == nullptr) { return; }
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

void will_engine::Scene::imguiSceneGraph()
{
    if (ImGui::Begin("Scene Graph")) {
        if (sceneRoot != nullptr && !sceneRoot->getChildren().empty()) {
            for (IHierarchical* child : sceneRoot->getChildren()) {
                displayGameObject(child, 0);
            }
        } else {
            ImGui::Text("Scene is empty");
        }
    }
    ImGui::End();
}

void will_engine::Scene::displayGameObject(IHierarchical* obj, const int32_t depth) // NOLINT(*-no-recursion)
{
    constexpr int32_t indentLength = 10.0f;
    constexpr float treeNodeWidth = 200.0f;
    constexpr float spacing = 5.0f;

    ImGui::PushID(obj);
    ImGui::Indent(static_cast<float>(depth * indentLength));

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (obj->getChildren().empty()) { flags |= ImGuiTreeNodeFlags_Leaf; }

    ImGui::SetNextItemWidth(treeNodeWidth);
    bool isOpen = ImGui::TreeNodeEx("##TreeNode", flags, "%s", obj->getName().data());
    //bool isOpen = ImGui::TreeNodeEx((void *) (intptr_t) obj->getId(), flags, obj->getName().data());

    ImGui::SameLine(treeNodeWidth + spacing);
    ImGui::BeginGroup();

    const IHierarchical* parent = obj->getParent();
    if (parent != nullptr) {
        constexpr float arrowWidth = 20.0f;
        constexpr float buttonWidth = 75.0f;
        const std::vector<IHierarchical*>& parentChildren = obj->getParent()->getChildren();

        if (parent != sceneRoot) {
            if (ImGui::Button("Undent", ImVec2(buttonWidth, 0))) { undent(obj); }
        } else { ImGui::Dummy(ImVec2(buttonWidth, 0)); }

        ImGui::SameLine(0, spacing);
        if (parentChildren[0] != obj) {
            if (ImGui::Button("Indent", ImVec2(buttonWidth, 0))) { indent(obj); }
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
        for (IHierarchical* child : obj->getChildren()) {
            displayGameObject(child, depth + 1);
        }
        ImGui::TreePop();
    }

    ImGui::Unindent(static_cast<float>(depth * indentLength));
    ImGui::PopID();
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
