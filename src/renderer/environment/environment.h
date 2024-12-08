//
// Created by William on 2024-08-25.
//

#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H
#include <string>
#include <vulkan/vulkan_core.h>

#include "src/renderer/vk_descriptor_buffer.h"
#include "src/renderer/vk_types.h"

class VulkanContext;
class EnvironmentDescriptorLayouts;
class Engine;

struct EquiToCubePushConstantData
{
    bool flipY;
    float pad;
    float pad2;
    float pad3;
};

struct CubeToDiffusePushConstantData
{
    float sampleDelta;
    float pad;
    float pad2;
    float pad3;
};

struct CubeToPrefilteredConstantData
{
    float roughness;
    uint32_t imageWidth;
    uint32_t imageHeight;
    uint32_t sampleCount;
};

struct EnvironmentMapData
{
    std::string sourcePath;
    AllocatedImage cubemapImage;
    AllocatedImage specDiffCubemap;
};


class Environment
{
public:
    static constexpr float DIFFUFE_SAMPLE_DELTA{0.025f};
    static constexpr int SPECULAR_SAMPLE_COUNT{2048};
    // 1024 == .4k HDR file
    static constexpr uint32_t CUBEMAP_RESOLUTION{1024};
    static constexpr bool FLIP_Y{false};
    static constexpr int MAX_ENVIRONMENT_MAPS{10};
    static constexpr int SPECULAR_PREFILTERED_MIP_LEVELS{6};
    static constexpr int DIFFUSE_IRRADIANCE_MIP_LEVEL{5};
    static const VkExtent3D SPECULAR_PREFILTERED_BASE_EXTENTS;
    static const VkExtent3D LUT_IMAGE_EXTENT;

    explicit Environment(Engine* creator, VulkanContext& context, EnvironmentDescriptorLayouts& environmentDescriptorLayouts);

    ~Environment();

    void loadEnvironment(const char* name, const char* path, int environmentMapIndex = 0);

    DescriptorBufferSampler& getEquiImageDescriptorBuffer() { return equiImageDescriptorBuffer; }
    DescriptorBufferSampler& getCubemapDescriptorBuffer() { return cubemapDescriptorBuffer; }

    const std::unordered_map<int32_t, const char*>& getActiveEnvironmentMapNames() { return activeEnvironmentMapNames; }

    DescriptorBufferSampler& getDiffSpecMapDescriptorBuffer() { return diffSpecMapDescriptorBuffer; }

    [[nodiscard]] uint32_t getEnvironmentMapOffset(const int32_t index) const
    {
        if (activeEnvironmentMapNames.contains(index)) {
            return cubemapDescriptorBuffer.getDescriptorBufferSize() * index;
        }
        return 0;
    }

    [[nodiscard]] uint32_t getDiffSpecMapOffset(const int32_t index) const
    {
        if (activeEnvironmentMapNames.contains(index)) {
            return diffSpecMapDescriptorBuffer.getDescriptorBufferSize() * index;
        }
        return 0;
    }

private:
    Engine* creator{};
    VulkanContext& context;
    EnvironmentDescriptorLayouts& descriptorLayouts;

    DescriptorBufferSampler equiImageDescriptorBuffer;
    DescriptorBufferSampler cubemapStorageDescriptorBuffer;
    DescriptorBufferSampler cubemapDescriptorBuffer;
    DescriptorBufferSampler lutDescriptorBuffer;
    DescriptorBufferSampler diffSpecMapDescriptorBuffer;

    // Pipelines
    VkPipelineLayout equiToCubemapPipelineLayout{VK_NULL_HANDLE};
    VkPipeline equiToCubemapPipeline{VK_NULL_HANDLE};

    VkPipelineLayout cubemapToDiffusePipelineLayout{VK_NULL_HANDLE};
    VkPipeline cubemapToDiffusePipeline{VK_NULL_HANDLE};
    VkPipelineLayout cubemapToSpecularPipelineLayout{VK_NULL_HANDLE};
    VkPipeline cubemapToSpecularPipeline{VK_NULL_HANDLE};

    // Hardcoded LUT generation
    VkPipelineLayout lutPipelineLayout{VK_NULL_HANDLE};
    VkPipeline lutPipeline{VK_NULL_HANDLE};
    AllocatedImage lutImage; // same for all environment maps
    VkSampler sampler{};


    std::unordered_map<int32_t, const char*> activeEnvironmentMapNames{};
    EnvironmentMapData environmentMaps[10]{
        {"", VK_NULL_HANDLE, VK_NULL_HANDLE},
        {"", VK_NULL_HANDLE, VK_NULL_HANDLE},
        {"", VK_NULL_HANDLE, VK_NULL_HANDLE},
        {"", VK_NULL_HANDLE, VK_NULL_HANDLE},
        {"", VK_NULL_HANDLE, VK_NULL_HANDLE},
        {"", VK_NULL_HANDLE, VK_NULL_HANDLE},
        {"", VK_NULL_HANDLE, VK_NULL_HANDLE},
        {"", VK_NULL_HANDLE, VK_NULL_HANDLE},
        {"", VK_NULL_HANDLE, VK_NULL_HANDLE},
        {"", VK_NULL_HANDLE, VK_NULL_HANDLE},
    };

    void equiToCubemapImmediate(const AllocatedImage& cubemapImage, int cubemapStorageDescriptorIndex) const;

    void cubemapToDiffuseSpecularImmediate(const AllocatedCubemap& cubemapMips, int cubemapSampleDescriptorIndex) const;

    void generateLutImmediate(const Engine* engine) const;
};


#endif //ENVIRONMENT_H
