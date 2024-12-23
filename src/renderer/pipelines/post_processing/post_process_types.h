//
// Created by William on 2024-12-23.
//

#ifndef POST_PROCESS_TYPES_H
#define POST_PROCESS_TYPES_H

#include <vulkan/vulkan_core.h>

enum class PostProcessType : uint32_t
{
    None = 0,
    Tonemapping = 1 << 0,
    Sharpening = 1 << 1,
};

inline PostProcessType operator|(PostProcessType a, PostProcessType b) {
    return static_cast<PostProcessType>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
    );
}

inline bool HasFlag(PostProcessType flags, PostProcessType flag)
{
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

#endif //POST_PROCESS_TYPES_H
