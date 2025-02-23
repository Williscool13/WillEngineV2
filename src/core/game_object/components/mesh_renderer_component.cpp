//
// Created by William on 2025-02-23.
//

#include "mesh_renderer_component.h"

#include <fmt/format.h>

#include "imgui.h"
#include "src/core/engine.h"
#include "src/core/game_object/transformable.h"

namespace will_engine::components
{
MeshRendererComponent::MeshRendererComponent(const std::string& name)
    : Component(name)
{}

MeshRendererComponent::~MeshRendererComponent() = default;

void MeshRendererComponent::dirty()
{
    if (!bIsStatic) {
        renderFramesToUpdate = FRAME_OVERLAP + 1;
    }
}

bool MeshRendererComponent::hasMesh() const
{
    return pRenderReference && meshIndex != INDEX_NONE;
}

void MeshRendererComponent::beginPlay()
{
    Component::beginPlay();
    // Add initialization code here
}

void MeshRendererComponent::update(const float deltaTime)
{
    Component::update(deltaTime);
    // Add update logic here
}

void MeshRendererComponent::releaseMesh()
{
    if (!pRenderReference) {
        meshIndex = INDEX_NONE;
        return;
    }
    pRenderReference->releaseInstanceIndex(this);
    pRenderReference = nullptr;
    meshIndex = INDEX_NONE;
}

void MeshRendererComponent::beginDestroy()
{
    Component::beginDestroy();
    releaseMesh();
}

void MeshRendererComponent::serialize(ordered_json& j)
{
    Component::serialize(j);

    const int32_t renderRefIndex = getRenderReferenceId();
    if (renderRefIndex != INDEX_NONE) {
        j["renderReference"] = renderRefIndex;
        j["renderMeshIndex"] = meshIndex;
        j["renderIsVisible"] = bIsVisible;
        j["renderIsShadowCaster"] = bIsShadowCaster;
    }
}

void MeshRendererComponent::deserialize(ordered_json& j)
{
    Component::deserialize(j);

    if (j.contains("renderReference") && j.contains("renderMeshIndex")) {
        const uint32_t renderRefIndex = j["renderReference"].get<uint32_t>();
        const int32_t meshIndex = j["renderMeshIndex"].get<int32_t>();

        if (RenderObject* renderObject = Engine::get()->getRenderObject(renderRefIndex)) {
            renderObject->generateMesh(this, meshIndex);
            if (j.contains("renderIsVisible")) {
                bIsVisible = j["renderIsVisible"];
            }
            if (j.contains("renderIsShadowCaster")) {
                bIsShadowCaster = j["renderIsShadowCaster"];
            }
        }
        else {
            fmt::print("Warning: Mesh Renderer Component failed to find render reference\n");
        }
    }
}

void MeshRendererComponent::updateRenderImgui()
{
    Component::updateRenderImgui();

    if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!hasMesh()) {
            ImGui::Text("Not currently representing any mesh");
        }
        else {
            ImGui::Text(fmt::format("Render Reference Id: {}",  pRenderReference->getId()).c_str());
            ImGui::Text(fmt::format("Mesh Index: {}",  meshIndex).c_str());

            if (ImGui::Button("Release Mesh")) {
                releaseMesh();
            }
            ImGui::Separator();

            ImGui::Checkbox("Visible", &bIsVisible);
            ImGui::Checkbox("Cast Shadows", &bIsShadowCaster);
        }
    }
}

glm::mat4 MeshRendererComponent::getModelMatrix()
{
    if (transformableOwner) {
        return transformableOwner->getModelMatrix();
    }

    return glm::mat4(1.0f);
}

void MeshRendererComponent::setOwner(IComponentContainer* owner)
{
    Component::setOwner(owner);

    transformableOwner = dynamic_cast<ITransformable*>(owner);
    if (!transformableOwner) {
        fmt::print("Attempted to attach a mesh renderer to an IComponentContainer that does not implement ITransformable. Model matrix will always be identity.\n");
    }
}
} // namespace will_engine::components
