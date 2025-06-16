//
// Created by William on 2025-02-23.
//

#ifndef MESH_RENDERER_COMPONENT_H
#define MESH_RENDERER_COMPONENT_H

#include <string_view>
#include <json/json.hpp>

#include "engine/core/game_object/renderable.h"
#include "engine/core/game_object/components/component.h"
#include "engine/renderer/renderer_constants.h"
#include "engine/renderer/pipelines/debug/debug_highlighter.h"
#include "engine/util/math_constants.h"

namespace will_engine
{
class ITransformable;
}

namespace will_engine::components
{
class MeshRendererComponent : public Component, public renderer::IRenderable
{
public:
    explicit MeshRendererComponent(const std::string& name = "");

    ~MeshRendererComponent() override;

public:
    bool hasMesh() const;

private: // IRenderable
    /**
      * If true, the model matrix will never be updated from defaults.
      */
    bool bIsStatic{false};
    bool bIsVisible{true};
    bool bIsShadowCaster{true};
    /**
     * The render object that is responsible for drawing this gameobject's model
     */
    renderer::IRenderReference* pRenderReference{nullptr};
    int32_t meshIndex{INDEX_NONE};

    int32_t renderFramesToUpdate{FRAME_OVERLAP + 1};

private:
    ITransformable* transformableOwner{nullptr};

public:
    void beginPlay() override;

    void update(float deltaTime) override;

    void beginDestroy() override;

public: // Debug Highlight
    virtual bool canDrawHighlight() override;

    virtual renderer::HighlightData getHighlightData() override;

public: // Serialization
    void serialize(ordered_json& j) override;

    void deserialize(ordered_json& j) override;

public: // Editor Tools
    void updateRenderImgui() override;

public: // IRenderable
    int32_t getRenderFramesToUpdate() override { return renderFramesToUpdate; }

    void dirty() override;

    void setRenderFramesToUpdate(const int32_t value) override { renderFramesToUpdate = value; }


    void setRenderObjectReference(renderer::IRenderReference* owner, const int32_t meshIndex) override
    {
        pRenderReference = owner;
        this->meshIndex = meshIndex;
    }

    uint32_t getRenderReferenceId() const override { return pRenderReference ? pRenderReference->getId() : INDEX_NONE; }

    renderer::IRenderReference* getRenderReference() const override { return pRenderReference; }

    int32_t getMeshIndex() const override { return meshIndex; }

    void releaseMesh() override;

    [[nodiscard]] bool& isVisible() override { return bIsVisible; }

    void setVisibility(const bool isVisible) override { bIsVisible = isVisible; }

    [[nodiscard]] bool& isShadowCaster() override { return bIsShadowCaster; }

    void setIsShadowCaster(const bool isShadowCaster) override { bIsShadowCaster = isShadowCaster; }

    void setTransform(const Transform& localTransform) override;

    glm::mat4 getModelMatrix() override;

    void setOwner(IComponentContainer* owner) override;

private:
    Transform localTransform{Transform::Identity};
    glm::mat4 cachedLocalModel{1.0f};

public:
    static constexpr auto TYPE = "MeshRendererComponent";
    static constexpr bool CAN_BE_CREATED_MANUALLY = true;

    static std::string_view getStaticType()
    {
        return TYPE;
    }

    std::string_view getComponentType() override { return TYPE; }
};
} // namespace will_engine::components

#endif // MESH_RENDERER_COMPONENT_H
