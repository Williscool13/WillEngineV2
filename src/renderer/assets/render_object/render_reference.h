//
// Created by William on 2025-01-24.
//

#ifndef RENDER_REFERENCE_H
#define RENDER_REFERENCE_H

#include <glm/glm.hpp>

namespace will_engine
{
class IRenderable;

struct CurrentInstanceData
{
    glm::mat4 currentModelMatrix{};
    bool bIsVisible{};
    bool bCastsShadows{true};
};

class IRenderReference
{
public:
    virtual ~IRenderReference() = default;

    [[nodiscard]] virtual uint32_t getId() const = 0;

    virtual bool releaseInstanceIndex(IRenderable* renderable) = 0;
};
}

#endif //RENDER_REFERENCE_H
