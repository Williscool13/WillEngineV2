//
// Created by William on 2025-06-17.
//

#ifndef RENDER_OBJECT_FWD_H
#define RENDER_OBJECT_FWD_H
#include "render_object.h"
#include "render_object_gltf.h"

namespace will_engine::renderer
{
using RenderObjectPtr = std::unique_ptr<RenderObject>;
}

#endif //RENDER_OBJECT_FWD_H
