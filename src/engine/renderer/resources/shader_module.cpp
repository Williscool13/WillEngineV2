//
// Created by William on 2025-05-30.
//

#include "shader_module.h"

#include <fstream>

#include <fmt/format.h>
#include <volk/volk.h>

#include "engine/renderer/resource_manager.h"

namespace will_engine::renderer
{
ShaderModule::ShaderModule(ResourceManager* resourceManager, const std::filesystem::path& path)
    : VulkanResource(resourceManager)
{
    if (path.extension() == ".spv") {
        loadSpvShader(path);
    }
    else {
        loadShader(path);
    }
}

ShaderModule::~ShaderModule()
{
    if (shader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(manager->getDevice(), shader, nullptr);
        shader = VK_NULL_HANDLE;
    }

}

void ShaderModule::loadSpvShader(const std::filesystem::path& path)
{
    const std::filesystem::path projectRoot = std::filesystem::current_path();

    const std::filesystem::path shaderPath((projectRoot / path).string().c_str());
    std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error(fmt::format("Failed to read file {}", shaderPath.string()));
    }

    const size_t fileSize = file.tellg();
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

    file.close();

    // create a new shader module, using the buffer we loaded
    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = buffer.size() * sizeof(uint32_t),
        .pCode = buffer.data(),
    };

    if (vkCreateShaderModule(manager->getDevice(), &createInfo, nullptr, &shader) != VK_SUCCESS) {
        throw std::runtime_error(fmt::format("Failed to load shader {}", path.filename().string()));
    }
}

void ShaderModule::loadShader(const std::filesystem::path& path)
{
    if (!exists(path)) {
        fmt::print("Shader source:\n{}\n", path.string());
        throw std::runtime_error("Source shader file not found");
    }
    std::ifstream file(path.string());
    auto source = std::string(std::istreambuf_iterator(file), std::istreambuf_iterator<char>());
    shaderc_shader_kind kind;
    if (path.extension() == ".vert") kind = shaderc_vertex_shader;
    else if (path.extension() == ".frag") kind = shaderc_fragment_shader;
    else if (path.extension() == ".comp") kind = shaderc_compute_shader;
    else if (path.extension() == ".tesc") kind = shaderc_tess_control_shader;
    else if (path.extension() == ".tese") kind = shaderc_tess_evaluation_shader;

    else throw std::runtime_error(fmt::format("Unknown shader type: {}", path.extension().string()));

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    std::vector<std::string> include_paths = {"shaders/include"};
    options.SetIncluder(std::make_unique<CustomIncluder>(include_paths));

    // Macros
    if (NORMAL_REMAP) {
        options.AddMacroDefinition("REMAP_NORMALS");
    }

    auto result = compiler.CompileGlslToSpv(source, kind, "shader", options);

    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        fmt::print("Shader source:\n{}\n", source);
        fmt::print("Compilation error:\n{}\n", result.GetErrorMessage());
        throw std::runtime_error(result.GetErrorMessage());
    }

    std::vector<uint32_t> spirv = {result.cbegin(), result.cend()};
    VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spirv.size() * sizeof(uint32_t),
        .pCode = spirv.data()
    };

    if (vkCreateShaderModule(manager->getDevice(), &createInfo, nullptr, &shader) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }
}
}
