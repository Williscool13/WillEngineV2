//
// Created by William on 2025-01-25.
//

#ifndef DEFERRED_RESOLVE_H
#define DEFERRED_RESOLVE_H
#include <vulkan/vulkan_core.h>

#include "src/renderer/descriptor_buffer/descriptor_buffer_sampler.h"


class ResourceManager;

namespace will_engine::deferred_resolve
{
struct DeferredResolveDescriptor
{
    VkImageView normalTarget;
    VkImageView albedoTarget;
    VkImageView pbrTarget;
    VkImageView depthTarget;
    VkImageView velocityTarget;
    VkImageView outputTarget;

    VkSampler sampler;
};

struct DeferredResolvePushConstants
{
    int32_t width{1700};
    int32_t height{900};
    int32_t deferredDebug{0};
    int32_t disableShadows{0};
    int32_t pcfLevel{5};
    float nearPlane{1000.0f};
    float farPlane{0.1f};
};

struct DeferredResolveDrawInfo
{
    int32_t deferredDebug{0};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
    VkDescriptorBufferBindingInfoEXT environmentIBLBinding{};
    VkDeviceSize environmentIBLOffset{0};
    VkDescriptorBufferBindingInfoEXT cascadeUniformBinding{};
    VkDeviceSize cascadeUniformOffset{0};
    VkDescriptorBufferBindingInfoEXT cascadeSamplerBinding{};
    float nearPlane{1000.0f};
    float farPlane{0.1f};
};

class DeferredResolvePipeline
{
public:
    explicit DeferredResolvePipeline(ResourceManager& resourceManager, VkDescriptorSetLayout environmentIBLLayout, VkDescriptorSetLayout cascadeUniformLayout,
                                     VkDescriptorSetLayout cascadeSamplerlayout);

    ~DeferredResolvePipeline();

    void setupDescriptorBuffer(const DeferredResolveDescriptor& drawInfo);

    void draw(VkCommandBuffer cmd, const DeferredResolveDrawInfo& drawInfo) const;

    void reloadShaders() { createPipeline(); }

private:
    void createPipeline();

private:
    ResourceManager& resourceManager;

    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};

    DescriptorBufferSampler resolveDescriptorBuffer;
};
}


#endif //DEFERRED_RESOLVE_H
