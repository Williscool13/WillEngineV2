//
// Created by William on 2025-01-25.
//

#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H
#include <vulkan/vulkan_core.h>

#include "engine/renderer/immediate_submitter.h"
#include "engine/renderer/vk_types.h"
#include "engine/renderer/resources/allocated_image.h"
#include "engine/renderer/resources/descriptor_set_layout.h"
#include "engine/renderer/resources/pipeline.h"
#include "engine/renderer/resources/pipeline_layout.h"
#include "engine/renderer/resources/sampler.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_sampler.h"

namespace will_engine::renderer
{
class ResourceManager;

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

struct EnvironmentMapEntry
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

    void loadEnvironment(const char* name, const char* path, int32_t environmentMapIndex = 0);

private:
    std::unordered_map<int32_t, const char*> activeEnvironmentMapNames{};
    EnvironmentMapEntry environmentMaps[11]{
        {"", {}, {}},
        {"", {}, {}},
        {"", {}, {}},
        {"", {}, {}},
        {"", {}, {}},
        {"", {}, {}},
        {"", {}, {}},
        {"", {}, {}},
        {"", {}, {}},
        {"", {}, {}},
    };

public:
    [[nodiscard]] VkDescriptorSetLayout getCubemapDescriptorSetLayout() const { return cubemapSamplerLayout.layout; }
    [[nodiscard]] VkDescriptorSetLayout getDiffSpecMapDescriptorSetlayout() const {return environmentIBLLayout.layout; }

    DescriptorBufferSampler& getCubemapDescriptorBuffer() { return cubemapDescriptorBuffer; }
    DescriptorBufferSampler& getDiffSpecMapDescriptorBuffer() { return diffSpecMapDescriptorBuffer; }

public: // Debug
    const std::unordered_map<int32_t, const char*>& getActiveEnvironmentMapNames() { return activeEnvironmentMapNames; }

private:
    ResourceManager& resourceManager;
    ImmediateSubmitter& immediate;

    DescriptorSetLayout equiImageLayout{};
    DescriptorSetLayout cubemapStorageLayout{};
    /**
     * Final cubemap, for environment rendering
     */
    DescriptorSetLayout cubemapSamplerLayout{};
    DescriptorSetLayout lutLayout{};
    /**
     * Final PBR data, used for pbr calculations (diff/spec and LUT)
     * Diff/Spec Irradiance Cubemap (LOD 1-4 spec, LOD 5 diff), and 2D-LUT
     */
    DescriptorSetLayout environmentIBLLayout{};

    DescriptorBufferSampler equiImageDescriptorBuffer;
    DescriptorBufferSampler cubemapStorageDescriptorBuffer;
    DescriptorBufferSampler cubemapDescriptorBuffer;
    DescriptorBufferSampler lutDescriptorBuffer;
    DescriptorBufferSampler diffSpecMapDescriptorBuffer;

    PipelineLayout equiToCubemapPipelineLayout{};
    Pipeline equiToCubemapPipeline{};

    PipelineLayout cubemapToDiffusePipelineLayout{};
    Pipeline cubemapToDiffusePipeline{};
    PipelineLayout cubemapToSpecularPipelineLayout{};
    Pipeline cubemapToSpecularPipeline{};

    // Hardcoded LUT generation
    PipelineLayout lutPipelineLayout{};
    Pipeline lutPipeline{};
    AllocatedImage lutImage; // same for all environment maps

    Sampler sampler{};
};
}


#endif //ENVIRONMENT_H
