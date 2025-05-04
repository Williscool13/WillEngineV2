//
// Created by William on 2025-01-01.
//

#ifndef RENDERER_CONSTANTS_H
#define RENDERER_CONSTANTS_H
#include <vulkan/vulkan_core.h>

namespace will_engine
{
constexpr int32_t FRAME_OVERLAP = 2;
constexpr char ENGINE_NAME[] = "Will Engine";
constexpr bool USING_REVERSED_DEPTH_BUFFER = true;
constexpr VkDeviceSize ZERO_DEVICE_SIZE = 0;
constexpr VkExtent2D RENDER_EXTENTS{1920, 1080};
//constexpr VkExtent2D RENDER_EXTENTS{2560 , 1440};
//constexpr VkExtent2D RENDER_EXTENTS{3840, 2160};
constexpr float RENDER_EXTENT_WIDTH{RENDER_EXTENTS.width};
constexpr float RENDER_EXTENT_HEIGHT{RENDER_EXTENTS.height};
constexpr VkFormat DRAW_FORMAT{VK_FORMAT_R16G16B16A16_SFLOAT};
constexpr VkFormat DEPTH_FORMAT{VK_FORMAT_D32_SFLOAT};
constexpr VkFormat VELOCITY_FORMAT{VK_FORMAT_R16G16_SFLOAT};
constexpr VkFormat DEBUG_FORMAT{VK_FORMAT_R8G8B8A8_SNORM};

constexpr bool NORMAL_REMAP{true};
constexpr VkFormat NORMAL_FORMAT = NORMAL_REMAP ? VK_FORMAT_A2R10G10B10_UNORM_PACK32 : VK_FORMAT_R16G16B16A16_SNORM;

// Be careful, environment map is in HDR format, so non-float formats for albedo won't work
constexpr VkFormat ALBEDO_FORMAT{VK_FORMAT_R16G16B16A16_SFLOAT};
constexpr VkFormat PBR_FORMAT{VK_FORMAT_R8G8B8A8_UNORM};
}


#endif // RENDERER_CONSTANTS_H
