//
// Created by William on 2025-01-24.
//

#ifndef RENDERABLE_H
#define RENDERABLE_H
#include <cstdint>

#include "glm/fwd.hpp"
#include "src/renderer/render_object/render_reference.h"

namespace will_engine
{
class IRenderable
{
public:
    virtual ~IRenderable() = default;

    virtual void setRenderObjectReference(IRenderReference* owner, int32_t index) = 0;
};
}
#endif //RENDERABLE_H
