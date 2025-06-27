//
// Created by William on 2025-01-24.
//

#ifndef RENDER_REFERENCE_H
#define RENDER_REFERENCE_H

#include <optional>

#include <glm/glm.hpp>

#include "render_object_types.h"

namespace will_engine::renderer
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

    virtual std::vector<Primitive> getPrimitives(int32_t meshIndex) = 0;

    virtual VkBuffer getPositionVertexBuffer() const = 0;

    virtual VkBuffer getPropertyVertexBuffer() const = 0;

    virtual VkBuffer getIndexBuffer() const = 0;
};
}

#endif //RENDER_REFERENCE_H
