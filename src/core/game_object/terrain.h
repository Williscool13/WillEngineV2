//
// Created by William on 2025-03-09.
//

#ifndef TERRAIN_H
#define TERRAIN_H
#include "src/renderer/vk_types.h"

namespace will_engine
{
class ITerrain
{
public:
    virtual ~ITerrain() = default;

    virtual AllocatedBuffer getVertexBuffer() = 0;

    virtual AllocatedBuffer getIndexBuffer() = 0;

    virtual size_t getIndicesCount() = 0;

    virtual bool canDraw() = 0;
};
}

#endif //TERRAIN_H
