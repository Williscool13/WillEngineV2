//
// Created by William on 2025-02-23.
//

#ifndef MESH_RENDERER_COMPONENT_H
#define MESH_RENDERER_COMPONENT_H

#include <string_view>
#include <json/json.hpp>

#include "src/core/game_object/renderable.h"
#include "src/core/game_object/components/component.h"
#include "src/renderer/renderer_constants.h"
#include "src/util/math_constants.h"

namespace will_engine
{
class ITransformable;
}

namespace will_engine::components
{
class MeshRendererComponent : public Component, public IRenderable
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
    IRenderReference* pRenderReference{nullptr};
    int32_t meshIndex{INDEX_NONE};

    int32_t renderFramesToUpdate{FRAME_OVERLAP + 1};

private:
    ITransformable* transformableOwner{nullptr};

public:
    void beginPlay() override;

    void update(float deltaTime) override;

    void beginDestroy() override;

public: // Serialization
    void serialize(ordered_json& j) override;

    void deserialize(ordered_json& j) override;

public: // Editor Tools
    void updateRenderImgui() override;

public: // IRenderable
    int32_t getRenderFramesToUpdate() override { return renderFramesToUpdate; }

    void dirty() override;

    void setRenderFramesToUpdate(const int32_t value) override { renderFramesToUpdate = value; }


    void setRenderObjectReference(IRenderReference* owner, const int32_t meshIndex) override
    {
        pRenderReference = owner;
        this->meshIndex = meshIndex;
    }

    uint32_t getRenderReferenceId() const override { return pRenderReference ? pRenderReference->getId() : INDEX_NONE; }

    int32_t getMeshIndex() const override { return meshIndex; }

    void releaseMesh() override;

    [[nodiscard]] bool& isVisible() override { return bIsVisible; }

    void setVisibility(const bool isVisible) override { bIsVisible = isVisible; }

    [[nodiscard]] bool& isShadowCaster() override { return bIsShadowCaster; }

    void setIsShadowCaster(const bool isShadowCaster) override { bIsShadowCaster = isShadowCaster; }

    glm::mat4 getModelMatrix() override;

    void setOwner(IComponentContainer* owner) override;

public:
    static constexpr auto TYPE = "MeshRendererComponent";

    static std::string_view getStaticType()
    {
        return TYPE;
    }

    std::string_view getComponentType() override { return TYPE; }
};
} // namespace will_engine::components

#endif // MESH_RENDERER_COMPONENT_H
