#include <iostream>

// vulkan
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vk_enum_string_helper.h>

// volk
#include <volk.h>

// fmt
#include <fmt/format.h>

// vma
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

// stbi
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

// vkb
#include <VkBootstrap.h>

// imgui
#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_vulkan.h>
#include <misc/cpp/imgui_stdlib.h>

// glm
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "src/core/engine.h"


int main(int argc, char* argv[])
{
    Engine engine;

    engine.init();

    engine.run();

    engine.cleanup();

    return 0;
}
