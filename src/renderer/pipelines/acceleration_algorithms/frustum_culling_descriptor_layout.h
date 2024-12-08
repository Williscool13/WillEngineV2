//
// Created by William on 2024-12-08.
//

#ifndef FRUSTUM_CULL_DESCRIPTOR_LAYOUT_H
#define FRUSTUM_CULL_DESCRIPTOR_LAYOUT_H


#include <volk.h>

class VulkanContext;

class FrustumCullingDescriptorLayouts
{
public:
    explicit FrustumCullingDescriptorLayouts(VulkanContext& context) : context(context)
    {
        createLayouts();
    }

    ~FrustumCullingDescriptorLayouts()
    {
        cleanup();
    }

    [[nodiscard]] VkDescriptorSetLayout getFrustumCullLayout() const { return frustumCullLayout; }

private:
    void createLayouts();

    void cleanup() const;

    VulkanContext& context;
    VkDescriptorSetLayout frustumCullLayout{VK_NULL_HANDLE};
};

#endif //FRUSTUM_CULL_DESCRIPTOR_LAYOUT_H
