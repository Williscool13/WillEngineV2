//
// Created by William on 2025-01-25.
//

#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H
#include <vulkan/vulkan_core.h>

#include "src/renderer/immediate_submitter.h"
#include "src/renderer/vk_types.h"
#include "src/renderer/descriptor_buffer/descriptor_buffer_sampler.h"

class ResourceManager;

namespace will_engine::environment
{
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
    Environment(ResourceManager& resourceManager, ImmediateSubmitter& immediate);

    ~Environment();

    void loadEnvironment(const char* name, const char* path, int environmentMapIndex = 0);

private:
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

public:
    [[nodiscard]] VkDescriptorSetLayout getCubemapDescriptorSetLayout() const { return cubemapSamplerLayout; }
    [[nodiscard]] VkDescriptorSetLayout getDiffSpecMapDescriptorSetlayout() const {return environmentIBLLayout; }

    DescriptorBufferSampler& getCubemapDescriptorBuffer() { return cubemapDescriptorBuffer; }
    DescriptorBufferSampler& getDiffSpecMapDescriptorBuffer() { return diffSpecMapDescriptorBuffer; }

private:
    ResourceManager& resourceManager;
    ImmediateSubmitter& immediate;

    VkDescriptorSetLayout equiImageLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout cubemapStorageLayout{VK_NULL_HANDLE};
    /**
     * Final cubemap, for environment rendering
     */
    VkDescriptorSetLayout cubemapSamplerLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout lutLayout{VK_NULL_HANDLE};
    /**
     * Final PBR data, used for pbr calculations (diff/spec and LUT)
     * Diff/Spec Irradiance Cubemap (LOD 1-4 spec, LOD 5 diff), and 2D-LUT
     */
    VkDescriptorSetLayout environmentIBLLayout{VK_NULL_HANDLE};

    DescriptorBufferSampler equiImageDescriptorBuffer;
    DescriptorBufferSampler cubemapStorageDescriptorBuffer;
    DescriptorBufferSampler cubemapDescriptorBuffer;
    DescriptorBufferSampler lutDescriptorBuffer;
    DescriptorBufferSampler diffSpecMapDescriptorBuffer;

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
};
}


#endif //ENVIRONMENT_H
