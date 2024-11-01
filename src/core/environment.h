//
// Created by William on 2024-08-25.
//

#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H
#include <string>
#include <unordered_set>
#include <vulkan/vulkan_core.h>

#include "../renderer/vk_descriptor_buffer.h"

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
    static constexpr int MAX_ENVIRONMENT_MAPS{10};
    static constexpr int specularPrefilteredMipLevels{6};
    static constexpr int diffuseIrradianceMipLevel{5};
    static const VkExtent3D specularPrefilteredBaseExtents;
    static const VkExtent3D lutImageExtent;
    static bool layoutsCreated;
    static int useCount;
    static VkDescriptorSetLayout equiImageDescriptorSetLayout;
    static VkDescriptorSetLayout cubemapStorageDescriptorSetLayout;
    /**
     * Final cubemap, for environment rendering
     */
    static VkDescriptorSetLayout cubemapDescriptorSetLayout;
    static VkDescriptorSetLayout lutDescriptorSetLayout;
    /**
     * Final PBR data, used for pbr calculations (diff/spec and LUT)
     */
    static VkDescriptorSetLayout environmentMapDescriptorSetLayout;

    explicit Environment(Engine* creator);

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
    std::unordered_map<int32_t, const char*> activeEnvironmentMapNames{};

private:
    Engine* creator{};

    bool flipY{false};

    float diffuse_sample_delta{0.025f};
    int specular_sample_count{2048};
    // 1024 == .4k HDR file
    uint32_t cubemapResolution{1024};

    DescriptorBufferSampler equiImageDescriptorBuffer;
    // the 2 below are identical, but one is for storage and one is for sampled
    DescriptorBufferSampler cubemapStorageDescriptorBuffer;
    DescriptorBufferSampler cubemapDescriptorBuffer;

    static DescriptorBufferSampler lutDescriptorBuffer;
    DescriptorBufferSampler diffSpecMapDescriptorBuffer;

    // Pipelines
    static VkPipelineLayout equiToCubemapPipelineLayout;
    static VkPipeline equiToCubemapPipeline;

    static VkPipelineLayout cubemapToDiffusePipelineLayout;
    static VkPipeline cubemapToDiffusePipeline;
    static VkPipelineLayout cubemapToSpecularPipelineLayout;
    static VkPipeline cubemapToSpecularPipeline;

    // Hardcoded LUT generation
    static VkPipelineLayout lutPipelineLayout;
    static VkPipeline lutPipeline;

    VkSampler sampler{};

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

    static AllocatedImage lutImage; // same for all environment maps


    void equiToCubemapImmediate(const AllocatedImage& cubemapImage, int cubemapStorageDescriptorIndex) const;

    void cubemapToDiffuseSpecularImmediate(AllocatedCubemap& cubemapMips, int cubemapSampleDescriptorIndex);

    static void generateLutImmediate(Engine* engine);

    static void initializeStatics(Engine* engine);
};


#endif //ENVIRONMENT_H
