//
// Created by William on 2024-09-03.
//

#ifndef VK_LOADER_H
#define VK_LOADER_H

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

#include "../core/engine.h"


class RenderObject;

namespace vk_loader {
    RenderObject* loadGltf(Engine* engine, std::string_view filepath);
};



#endif //VK_LOADER_H
