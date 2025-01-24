//
// Created by William on 2025-01-24.
//

#ifndef RENDER_REFERENCE_H
#define RENDER_REFERENCE_H

#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>

namespace will_engine
{
class IRenderReference
{
public:
    virtual ~IRenderReference() = default;

    virtual void updateInstanceData(int32_t instanceIndex, const glm::mat4& currentFrameModelMatrix) = 0;
};
}

#endif //RENDER_REFERENCE_H
