//
// Created by William on 2025-04-27.
//

#ifndef DEBUG_RENDERER_TYPES_H
#define DEBUG_RENDERER_TYPES_H

#include <glm/glm.hpp>
#include <volk/volk.h>

namespace will_engine::debug_renderer
{
static constexpr inline int32_t DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT = 512;
enum class DebugRendererCategory : uint32_t
{
    None = 0x00000000, // Will always be drawn
    Physics = 1 << 0,
    Gameplay = 1 << 1,
    UI = 1 << 2,
    General = 1 << 3,
    Navigation = 1 << 4,
    AI = 1 << 5,
    ALL = 0xFFFFFFFF
};

inline DebugRendererCategory operator|(DebugRendererCategory a, DebugRendererCategory b)
{
    return static_cast<DebugRendererCategory>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline DebugRendererCategory operator&(DebugRendererCategory a, DebugRendererCategory b)
{
    return static_cast<DebugRendererCategory>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline DebugRendererCategory operator~(DebugRendererCategory a)
{
    return static_cast<DebugRendererCategory>(~static_cast<uint32_t>(a));
}

inline DebugRendererCategory& operator|=(DebugRendererCategory& a, DebugRendererCategory b)
{
    return a = a | b;
}

inline DebugRendererCategory& operator&=(DebugRendererCategory& a, DebugRendererCategory b)
{
    return a = a & b;
}

inline bool hasFlag(DebugRendererCategory flags, DebugRendererCategory flag)
{
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) == static_cast<uint32_t>(flag);
}

struct DebugRendererVertex
{
    glm::vec3 position;
};

struct BoxInstance {
    glm::mat4 transform;  // Position + scale
    glm::vec3 color;
    float padding;
};

struct DebugRendererDrawInfo
{
    int32_t currentFrameOverlap{};
    VkImageView albedoTarget;
    VkImageView depthTarget;
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
};
}

#endif //DEBUG_RENDERER_TYPES_H
