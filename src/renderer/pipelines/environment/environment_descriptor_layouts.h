//
// Created by William on 2024-12-08.
//

#ifndef ENVIRONMENT_DESCRIPTOR_LAYOUTS_H
#define ENVIRONMENT_DESCRIPTOR_LAYOUTS_H

#include <vulkan/vulkan_core.h>

#include "src/renderer/vulkan_context.h"

class EnvironmentDescriptorLayouts
{
public:
    explicit EnvironmentDescriptorLayouts(VulkanContext& context) : context(context)
    {
        createLayouts();
    }

    ~EnvironmentDescriptorLayouts()
    {
        cleanup();
    }

    [[nodiscard]] VkDescriptorSetLayout getEquiImageLayout() const { return equiImageLayout; }
    [[nodiscard]] VkDescriptorSetLayout getCubemapStorageLayout() const { return cubemapStorageLayout; }
    [[nodiscard]] VkDescriptorSetLayout getCubemapSamplerLayout() const { return cubemapSamplerLayout; }
    [[nodiscard]] VkDescriptorSetLayout getLutLayout() const { return lutLayout; }
    [[nodiscard]] VkDescriptorSetLayout getEnvironmentMapLayout() const { return environmentMapLayout; }

private:
    void createLayouts();

    void cleanup() const;

    VulkanContext& context;

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
    VkDescriptorSetLayout environmentMapLayout{VK_NULL_HANDLE};
};


#endif //ENVIRONMENT_DESCRIPTOR_LAYOUTS_H
