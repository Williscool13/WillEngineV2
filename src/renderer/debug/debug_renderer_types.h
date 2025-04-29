//
// Created by William on 2025-04-27.
//

#ifndef DEBUG_RENDERER_TYPES_H
#define DEBUG_RENDERER_TYPES_H

#include <glm/glm.hpp>
#include <volk/volk.h>

#include "src/renderer/renderer_constants.h"
#include "src/renderer/vk_types.h"
#include "src/renderer/descriptor_buffer/descriptor_buffer_uniform.h"

namespace will_engine::debug_renderer
{
static constexpr inline int32_t DEFAULT_DEBUG_RENDERER_INSTANCE_COUNT = 512;
static constexpr inline int32_t BOX_INSTANCE_INDEX = 0;
static constexpr inline int32_t SPHERE_INSTANCE_INDEX = 1;

struct SphereDetailLevel {
    int32_t rings;
    int32_t segments;
};

static constexpr inline SphereDetailLevel SPHERE_DETAIL_LOW = {4, 8};
static constexpr inline SphereDetailLevel SPHERE_DETAIL_MEDIUM = {8, 16};
static constexpr inline SphereDetailLevel SPHERE_DETAIL_HIGH = {16, 32};

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

struct DebugRendererVertexFull
{
    glm::vec3 position;
    glm::vec3 color;
};

struct DebugRendererVertex
{
    glm::vec3 position;
};

struct DebugRendererInstance
{
    glm::mat4 transform{1.0f}; // Position + scale
    glm::vec3 color{0.0f, 1.0f, 0.0f};
    float padding{0.0f};
};

struct DebugRendererDrawInfo
{
    int32_t currentFrameOverlap{};
    VkImageView albedoTarget;
    VkImageView depthTarget;
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
};

struct DrawIndexedData
{
    uint32_t indexCount;
    uint32_t firstIndex;
    int32_t vertexOffset;
    uint32_t firstInstance;
};


struct DebugRenderGroup
{
    std::vector<DebugRendererInstance> instances;
    std::array<AllocatedBuffer, FRAME_OVERLAP> instanceBuffers{VK_NULL_HANDLE, VK_NULL_HANDLE};
    std::array<uint64_t, FRAME_OVERLAP> instanceBufferSizes{0, 0};
    DrawIndexedData drawIndexedData{};
    DescriptorBufferUniform instanceDescriptorBuffer;

};
}

#endif //DEBUG_RENDERER_TYPES_H
