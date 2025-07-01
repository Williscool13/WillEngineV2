//
// Created by William on 2025-02-23.
//

#include "mesh_renderer_component.h"

#include <fmt/format.h>

#include "imgui.h"
#include "engine/core/engine.h"
#include "engine/core/game_object/transformable.h"

namespace will_engine::game
{
MeshRendererComponent::MeshRendererComponent(const std::string& name)
    : Component(name)
{}

MeshRendererComponent::~MeshRendererComponent() = default;

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

void MeshRendererComponent::beginDestroy()
{
    Component::beginDestroy();
    releaseMesh();
}

renderer::HighlightData MeshRendererComponent::getHighlightData()
{
    if (!pRenderReference) {
        return {};
    }

    renderer::HighlightData data{};
    data.primitives = pRenderReference->getPrimitives(meshIndex);
    data.vertexBuffer = pRenderReference->getPositionVertexBuffer();
    data.indexBuffer = pRenderReference->getIndexBuffer();
    data.modelMatrix = getModelMatrix();
    return data;
}


void MeshRendererComponent::serialize(ordered_json& j)
{
    Component::serialize(j);

    const uint32_t renderRefIndex = getRenderReferenceId();
    if (renderRefIndex != INDEX_NONE) {
        j["renderReference"] = renderRefIndex;
        j["renderMeshIndex"] = meshIndex;
        j["renderIsVisible"] = bIsVisible;
        j["renderIsShadowCaster"] = bIsShadowCaster;
    }

    j["transform"] = localTransform;
}

void MeshRendererComponent::deserialize(ordered_json& j)
{
    Component::deserialize(j);

    if (j.contains("renderReference") && j.contains("renderMeshIndex")) {
        const uint32_t renderRefIndex = j["renderReference"].get<uint32_t>();
        const int32_t meshIndex = j["renderMeshIndex"].get<int32_t>();

        if (const renderer::AssetManager* assetManager = Engine::get()->getAssetManager()) {
            if (renderer::RenderObject* renderObject = assetManager->getRenderObject(renderRefIndex)) {
                if (!renderObject->isLoaded()) {
                    renderObject->load();
                }
                renderObject->generateMesh(this, meshIndex);
                if (j.contains("renderIsVisible")) {
                    bIsVisible = j["renderIsVisible"];
                }
                if (j.contains("renderIsShadowCaster")) {
                    bIsShadowCaster = j["renderIsShadowCaster"];
                }
            } else {
                fmt::print("Warning: Mesh Renderer Component failed to find render reference\n");
            }
        }
    }

    if (j.contains("transform")) {
        setTransform(j["transform"]);
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
            ImGui::Text(fmt::format("Render Reference Id: {}", pRenderReference->getId()).c_str());
            ImGui::Text(fmt::format("Mesh Index: {}", meshIndex).c_str());

            if (ImGui::Button("Release Mesh")) {
                releaseMesh();
            }
            ImGui::Separator();

            const bool originalVis = bIsVisible;
            const bool originalShadow = bIsShadowCaster;
            ImGui::Checkbox("Visible", &bIsVisible);
            ImGui::Checkbox("Cast Shadows", &bIsShadowCaster);

            if (bIsVisible != originalVis || bIsShadowCaster != originalShadow) {
                dirty();
            }
        }
    }

    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool transformChanged = false;

        glm::vec3 position = localTransform.getPosition();
        const glm::quat rotation = localTransform.getRotation();
        glm::vec3 scale = localTransform.getScale();

        glm::vec3 eulerRotation = glm::degrees(glm::eulerAngles(rotation));

        transformChanged |= ImGui::DragFloat3("Position", &position.x, 0.1f);
        transformChanged |= ImGui::DragFloat3("Rotation", &eulerRotation.x, 0.5f);
        transformChanged |= ImGui::DragFloat3("Scale", &scale.x, 0.01f, 0.001f, 100.0f);

        if (transformChanged) {
            localTransform.setPosition(position);
            localTransform.setRotation(glm::quat(glm::radians(eulerRotation)));
            localTransform.setScale(scale);
            cachedLocalModel = localTransform.toModelMatrix();
            dirty();
        }
    }
}

void MeshRendererComponent::dirty()
{
    if (!bIsStatic) {
        renderFramesToUpdate = FRAME_OVERLAP + 1;
    }
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

void MeshRendererComponent::setTransform(const Transform& localTransform)
{
    this->localTransform = localTransform;
    cachedLocalModel = localTransform.toModelMatrix();
}

glm::mat4 MeshRendererComponent::getModelMatrix()
{
    if (transformableOwner) {
        return transformableOwner->getModelMatrix() * cachedLocalModel;
    }

    return cachedLocalModel;
}

void MeshRendererComponent::setOwner(IComponentContainer* owner)
{
    Component::setOwner(owner);

    transformableOwner = dynamic_cast<ITransformable*>(owner);
    if (!transformableOwner) {
        fmt::print(
            "Attempted to attach a mesh renderer to an IComponentContainer that does not implement ITransformable. Model matrix will always be identity.\n");
    }
}
} // namespace will_engine::components
