//
// Created by William on 2025-01-24.
//

#ifndef RENDER_REFERENCE_H
#define RENDER_REFERENCE_H

#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>

#include "src/core/identifier/identifiable.h"

namespace will_engine
{
struct CurrentInstanceData
{
    glm::mat4 currentModelMatrix{};
    bool bIsVisible{};
    bool bCastsShadows{true};
};

class IRenderReference : public IIdentifiable
{
public:
    ~IRenderReference() override = default;

    void setId(uint64_t identifier) override = 0;

    [[nodiscard]] uint64_t getId() const override = 0;

    [[nodiscard]] virtual uint64_t getRenderReferenceIndex() const = 0;

    virtual void updateInstanceData(int32_t instanceIndex, const CurrentInstanceData& newInstanceData, int32_t currentFrameOverlap, int32_t previousFrameOverlap) = 0;

    virtual bool releaseInstanceIndex(uint32_t instanceIndex) = 0;
};
}

#endif //RENDER_REFERENCE_H
