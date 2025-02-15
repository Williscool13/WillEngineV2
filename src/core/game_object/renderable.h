//
// Created by William on 2025-01-24.
//

#ifndef RENDERABLE_H
#define RENDERABLE_H
#include <cstdint>

#include "src/renderer/render_object/render_reference.h"

namespace will_engine
{
class IRenderable
{
public:
    virtual ~IRenderable() = default;

    virtual void setRenderObjectReference(IRenderReference* owner, int32_t instanceIndex, int32_t meshIndex) = 0;

    [[nodiscard]] virtual uint32_t getRenderReferenceIndex() const = 0;

    [[nodiscard]] virtual int32_t getMeshIndex() const = 0;

    [[nodiscard]] virtual bool& isVisible() = 0;

    virtual void setVisibility(bool isVisible) = 0;

    [[nodiscard]] virtual bool& isShadowCaster() = 0;

    virtual void setIsShadowCaster(bool isShadowCaster) = 0;
};
}
#endif //RENDERABLE_H
