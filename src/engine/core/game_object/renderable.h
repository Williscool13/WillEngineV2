//
// Created by William on 2025-01-24.
//

#ifndef RENDERABLE_H
#define RENDERABLE_H
#include <cstdint>

#include "engine/renderer/assets/render_object/render_reference.h"
#include "engine/renderer/pipelines/debug/debug_highlight_types.h"

namespace will_engine::renderer
{
class IRenderable
{
public:
    virtual ~IRenderable() = default;

    virtual void setRenderObjectReference(IRenderReference* owner, int32_t meshIndex) = 0;

    virtual void releaseMesh() = 0;

    [[nodiscard]] virtual uint32_t getRenderReferenceId() const = 0;

    [[nodiscard]] virtual IRenderReference* getRenderReference() const = 0;

    [[nodiscard]] virtual int32_t getMeshIndex() const = 0;

    [[nodiscard]] virtual bool& isVisible() = 0;

    virtual void setVisibility(bool isVisible) = 0;

    [[nodiscard]] virtual bool& isShadowCaster() = 0;

    virtual void setIsShadowCaster(bool isShadowCaster) = 0;

    virtual void setTransform(const Transform& localTransform) = 0;

    virtual glm::mat4 getModelMatrix() = 0;

    virtual int32_t getRenderFramesToUpdate() = 0;

    virtual void dirty() = 0;

    virtual void setRenderFramesToUpdate(int32_t value) = 0;

    // Debug Highlight
    virtual bool canDrawHighlight() = 0;

    virtual HighlightData getHighlightData() = 0;
};
}
#endif //RENDERABLE_H
