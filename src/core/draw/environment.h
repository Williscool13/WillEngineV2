//
// Created by William on 2024-08-25.
//

#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H
#include <string>
#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>

#include "../../renderer/vk_descriptor_buffer.h"

class Engine;


struct EquiToCubePushConstantData {
	bool flipY;
	float pad;
	float pad2;
	float pad3;
};

struct CubeToDiffusePushConstantData {
	float sampleDelta;
	float pad;
	float pad2;
	float pad3;
};

struct CubeToPrefilteredConstantData {
	float roughness;
	uint32_t imageWidth;
	uint32_t imageHeight;
	uint32_t sampleCount;
};

struct EnvironmentMapData {
	std::string sourcePath;
	AllocatedImage cubemapImage;
	AllocatedImage specDiffCubemap;
};

struct EnvironmentSceneData
{
	glm::mat4 viewproj;
};


class Environment {
public:
	static const int MAX_ENVIRONMENT_MAPS{ 10 };
	static const int specularPrefilteredMipLevels{ 6 };
	static const VkExtent3D specularPrefilteredBaseExtents;
	static const VkExtent3D lutImageExtent;
	static const int diffuseIrradianceMipLevel{ 5 };
	static const char* defaultEquiPath;

	explicit Environment(Engine* creator);
	~Environment();

	// init sampler
	void loadCubemap(const char* path, int environmentMapIndex = 0);

	bool flip_y{ true };

	float diffuse_sample_delta{ 0.025f };
	int specular_sample_count{ 2048 };


	static bool layoutsCreated;
	static int useCount;
	static VkDescriptorSetLayout equiImageDescriptorSetLayout;
	static VkDescriptorSetLayout cubemapStorageDescriptorSetLayout;
	static VkDescriptorSetLayout cubemapDescriptorSetLayout;
	static VkDescriptorSetLayout lutDescriptorSetLayout;

	static VkDescriptorSetLayout environmentMapDescriptorSetLayout; // contains 2 samplers -> diffuse/spec and lut

	DescriptorBufferSampler& getEquiImageDescriptorBuffer() { return equiImageDescriptorBuffer; }
	DescriptorBufferSampler& getCubemapDescriptorBuffer() { return cubemapDescriptorBuffer; }

	DescriptorBufferSampler& getDiffSpecMapDescriptorBuffer() { return diffSpecMapDescriptorBuffer; }

private:
	Engine* creator{};
	VkDevice device{};
	VmaAllocator allocator{};


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

	VkSampler sampler{};

	//std::string _equiPath;
	uint32_t cubemapResolution{ 1024 };


	void equiToCubemapImmediate(AllocatedImage& cubemapImage, int cubemapStorageDescriptorIndex);
	void cubemapToDiffuseSpecularImmediate(AllocatedCubemap& cubemapMips, int cubemapSampleDescriptorIndex);
	static void generateLutImmediate(Engine* engine);

	static void initializeStatics(Engine* engine);
};



#endif //ENVIRONMENT_H
