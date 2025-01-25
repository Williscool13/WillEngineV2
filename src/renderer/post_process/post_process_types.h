//
// Created by William on 2025-01-25.
//

#ifndef POST_PROCESS_TYPES_H
#define POST_PROCESS_TYPES_H

#include <vulkan/vulkan_core.h>

namespace post_process
{
enum class PostProcessType : uint32_t
{
    None = 0,
    Tonemapping = 1 << 0,
    Sharpening = 1 << 1,
    ALL = Tonemapping | Sharpening
};

}

#endif //POST_PROCESS_TYPES_H
