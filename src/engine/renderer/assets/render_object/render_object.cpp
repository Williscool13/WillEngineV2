//
// Created by William on 2025-06-17.
//

#include "render_object.h"

namespace will_engine::renderer
{
RenderObject::RenderObject(ResourceManager& resourceManager, const RenderObjectInfo& renderObjectInfo)
    : resourceManager(resourceManager), renderObjectInfo(renderObjectInfo)
{}

RenderObject::~RenderObject()
{}
}
