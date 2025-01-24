//
// Created by William on 2025-01-01.
//

#ifndef RENDERER_CONSTANTS_H
#define RENDERER_CONSTANTS_H
#include <vulkan/vulkan_core.h>

constexpr unsigned int FRAME_OVERLAP = 2;
constexpr char ENGINE_NAME[] = "Will Engine";
constexpr bool USING_REVERSED_DEPTH_BUFFER = true;
constexpr VkDeviceSize ZERO_DEVICE_SIZE = 0;
constexpr VkExtent2D RENDER_EXTENTS{1920, 1080};

constexpr VkFormat DRAW_FORMAT{VK_FORMAT_R16G16B16A16_SFLOAT};
constexpr VkFormat DEPTH_FORMAT{VK_FORMAT_D32_SFLOAT};
constexpr VkFormat VELOCITY_FORMAT{VK_FORMAT_R16G16_SFLOAT};
constexpr VkFormat NORMAL_FORMAT{VK_FORMAT_R8G8B8A8_SNORM};
//constexpr VkFormat NORMAL_FORMAT{VK_FORMAT_R16G16B16A16_SNORM}; //VK_FORMAT_R8G8B8A8_SNORM - 8888 is too inaccurate for normals
constexpr VkFormat ALBEDO_FORMAT{VK_FORMAT_R8G8B8A8_UNORM};
constexpr VkFormat PBR_FORMAT{VK_FORMAT_R8G8B8A8_UNORM};

#endif // RENDERER_CONSTANTS_H
