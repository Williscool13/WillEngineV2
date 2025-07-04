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
/**
 * Default extents to use as a struct default parameter
 */
constexpr VkExtent2D DEFAULT_RENDER_EXTENT_2D{1920, 1080};
constexpr VkExtent3D DEFAULT_RENDER_EXTENT_3D{1920, 1080, 1};


// Be careful, environment map is in HDR format, so non-float formats for draw image won't work
constexpr VkFormat DRAW_FORMAT{VK_FORMAT_R16G16B16A16_SFLOAT};

constexpr VkFormat DEPTH_STENCIL_FORMAT{VK_FORMAT_D32_SFLOAT_S8_UINT};
constexpr VkFormat DEPTH_FORMAT{VK_FORMAT_D32_SFLOAT};
constexpr VkFormat STENCIL_FORMAT{VK_FORMAT_S8_UINT};

constexpr bool NORMAL_REMAP{true};
constexpr VkFormat NORMAL_FORMAT = NORMAL_REMAP ? VK_FORMAT_A2R10G10B10_UNORM_PACK32 : VK_FORMAT_R16G16B16A16_SNORM;

constexpr VkFormat VELOCITY_FORMAT{VK_FORMAT_R16G16_SFLOAT};

constexpr VkFormat DEBUG_FORMAT{VK_FORMAT_R8G8B8A8_SNORM};

// Be careful, environment map is in HDR format, so non-float formats for albedo won't work
constexpr VkFormat ALBEDO_FORMAT{VK_FORMAT_R16G16B16A16_SFLOAT};

constexpr VkFormat PBR_FORMAT{VK_FORMAT_R8G8B8A8_UNORM};
}


#endif // RENDERER_CONSTANTS_H
