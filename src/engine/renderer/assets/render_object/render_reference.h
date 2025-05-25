//
// Created by William on 2025-01-24.
//

#ifndef RENDER_REFERENCE_H
#define RENDER_REFERENCE_H

#include <optional>

#include <glm/glm.hpp>

#include "engine/renderer/resources/allocated_buffer.h"
#include "engine/renderer/vk_types.h"
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

    virtual std::optional<std::reference_wrapper<const Mesh>> getMeshData(int32_t meshIndex) = 0;

    virtual const AllocatedBuffer& getPositionVertexBuffer() const = 0;

    virtual const AllocatedBuffer& getPropertyVertexBuffer() const = 0;

    virtual const AllocatedBuffer& getIndexBuffer() const = 0;
};
}

#endif //RENDER_REFERENCE_H
