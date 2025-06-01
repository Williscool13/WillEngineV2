//
// Created by William on 2025-05-30.
//

#ifndef SHADER_MODULE_H
#define SHADER_MODULE_H
#include <filesystem>
#include <vulkan/vulkan_core.h>

#include "vulkan_resource.h"

namespace will_engine::renderer
{
struct ShaderModule : VulkanResource
{
    VkShaderModule shader{VK_NULL_HANDLE};

    void loadSpvShader(const std::filesystem::path& path);

    void loadShader(const std::filesystem::path& path);

    ShaderModule(ResourceManager* resourceManager, const std::filesystem::path& path);

    ~ShaderModule() override;
};
}

#endif //SHADER_MODULE_H
