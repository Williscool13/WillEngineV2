//
// Created by William on 2025-01-01.
//

#ifndef RENDERER_CONSTANTS_H
#define RENDERER_CONSTANTS_H
#include <vulkan/vulkan_core.h>

constexpr int32_t FRAME_OVERLAP = 2;
constexpr char ENGINE_NAME[] = "Will Engine";
constexpr bool USING_REVERSED_DEPTH_BUFFER = true;
constexpr VkDeviceSize ZERO_DEVICE_SIZE = 0;
constexpr VkExtent2D RENDER_EXTENTS{1920, 1080};
//constexpr VkExtent2D RENDER_EXTENTS{2140, 1440};
//constexpr VkExtent2D RENDER_EXTENTS{3840, 2160};
constexpr float RENDER_EXTENT_WIDTH{RENDER_EXTENTS.width};
constexpr float RENDER_EXTENT_HEIGHT{RENDER_EXTENTS.height};

constexpr VkFormat DRAW_FORMAT{VK_FORMAT_R16G16B16A16_SFLOAT};
constexpr VkFormat DEPTH_FORMAT{VK_FORMAT_D32_SFLOAT};
constexpr VkFormat VELOCITY_FORMAT{VK_FORMAT_R16G16_SFLOAT};
constexpr VkFormat NORMAL_FORMAT{VK_FORMAT_R16G16B16A16_SNORM};
// Be careful, environment map is in HDR format, so non-float formats wont work
constexpr VkFormat ALBEDO_FORMAT{VK_FORMAT_R16G16B16A16_SFLOAT};
constexpr VkFormat PBR_FORMAT{VK_FORMAT_R8G8B8A8_UNORM};

#endif // RENDERER_CONSTANTS_H
