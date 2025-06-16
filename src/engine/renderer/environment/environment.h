//
// Created by William on 2025-01-25.
//

#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H
#include <string>
#include <unordered_map>
#include <vulkan/vulkan_core.h>

#include "engine/renderer/resources/descriptor_set_layout.h"
#include "engine/renderer/resources/resources_fwd.h"

namespace will_engine::renderer
{
class ImmediateSubmitter;
class ResourceManager;

struct EnvironmentCubemapView
{
    ImageViewPtr imageView;
    VkExtent3D imageExtent;
    float roughness;
    int32_t descriptorBufferIndex;
};

struct EnvironmentCubemap
{
    VkImage image;
    VkFormat imageFormat;
    std::vector<EnvironmentCubemapView> cubemapImageViews; // one for each active mip level
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

struct EnvironmentMapEntry
{
    std::string sourcePath;
    ImageResourcePtr cubemapImage;
    ImageResourcePtr specDiffCubemap;
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
        {"", {nullptr}, {nullptr}},
        {"", {nullptr}, {nullptr}},
        {"", {nullptr}, {nullptr}},
        {"", {nullptr}, {nullptr}},
        {"", {nullptr}, {nullptr}},
        {"", {nullptr}, {nullptr}},
        {"", {nullptr}, {nullptr}},
        {"", {nullptr}, {nullptr}},
        {"", {nullptr}, {nullptr}},
        {"", {nullptr}, {nullptr}},
    };

public:
    [[nodiscard]] VkDescriptorSetLayout getCubemapDescriptorSetLayout() const { return cubemapSamplerLayout->layout; }
    [[nodiscard]] VkDescriptorSetLayout getDiffSpecMapDescriptorSetLayout() const {return environmentIBLLayout->layout; }

    DescriptorBufferSampler* getCubemapDescriptorBuffer() const { return cubemapDescriptorBuffer.get(); }
    DescriptorBufferSampler* getDiffSpecMapDescriptorBuffer() const { return diffSpecMapDescriptorBuffer.get(); }

public: // Debug
    const std::unordered_map<int32_t, const char*>& getActiveEnvironmentMapNames() { return activeEnvironmentMapNames; }

private:
    ResourceManager& resourceManager;
    ImmediateSubmitter& immediate;

    DescriptorSetLayoutPtr equiImageLayout{nullptr};
    DescriptorSetLayoutPtr cubemapStorageLayout{nullptr};
    /**
     * Final cubemap, for skybox rendering
     */
    DescriptorSetLayoutPtr cubemapSamplerLayout{nullptr};
    DescriptorSetLayoutPtr lutLayout{nullptr};
    /**
     * Final PBR data, used for pbr calculations (diff/spec and LUT)
     * Diff/Spec Irradiance Cubemap (LOD 1-4 specular, LOD 5 diffuse), and 2D-LUT
     */
    DescriptorSetLayoutPtr environmentIBLLayout{nullptr};

    DescriptorBufferSamplerPtr equiImageDescriptorBuffer;
    DescriptorBufferSamplerPtr cubemapStorageDescriptorBuffer;
    DescriptorBufferSamplerPtr cubemapDescriptorBuffer;
    DescriptorBufferSamplerPtr lutDescriptorBuffer;
    DescriptorBufferSamplerPtr diffSpecMapDescriptorBuffer;

    PipelineLayoutPtr equiToCubemapPipelineLayout{};
    PipelinePtr equiToCubemapPipeline{};

    PipelineLayoutPtr cubemapToDiffusePipelineLayout{};
    PipelinePtr cubemapToDiffusePipeline{};
    PipelineLayoutPtr cubemapToSpecularPipelineLayout{};
    PipelinePtr cubemapToSpecularPipeline{};

    // Hardcoded LUT generation
    PipelineLayoutPtr lutPipelineLayout{};
    PipelinePtr lutPipeline{};
    ImageResourcePtr lutImage; // same for all environment maps

    SamplerPtr sampler{};
};
}


#endif //ENVIRONMENT_H
