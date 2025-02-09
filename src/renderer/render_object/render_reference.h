//
// Created by William on 2025-01-24.
//

#ifndef RENDER_REFERENCE_H
#define RENDER_REFERENCE_H

#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>

namespace will_engine
{
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

    [[nodiscard]] virtual int32_t getRenderReferenceIndex() const = 0;

    virtual void updateInstanceData(int32_t instanceIndex, const CurrentInstanceData& newInstanceData, int32_t currentFrameOverlap, int32_t previousFrameOverlap) = 0;
};
}

#endif //RENDER_REFERENCE_H
