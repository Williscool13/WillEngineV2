//
// Created by William on 2025-01-24.
//

#ifndef RENDERABLE_H
#define RENDERABLE_H
#include <cstdint>

#include "glm/fwd.hpp"

namespace will_engine
{
class IRenderable
{
public:
    virtual ~IRenderable() = default;

    virtual void updateInstanceData(int32_t instanceIndex, const glm::mat4& instanceData) = 0;
};
}
#endif //RENDERABLE_H
