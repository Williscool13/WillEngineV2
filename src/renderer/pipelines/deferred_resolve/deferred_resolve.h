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
    int32_t debug{0};
    int32_t disableShadows{0};
    int32_t pcfLevel{5};
};

struct DeferredResolveDrawInfo
{
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
    VkDescriptorBufferBindingInfoEXT environmentIBLBinding{};
    VkDeviceSize environmentIBLOffset{0};
};

class DeferredResolvePipeline
{
public:
    explicit DeferredResolvePipeline(ResourceManager& resourceManager, VkDescriptorSetLayout environmentIBLLayout);

    ~DeferredResolvePipeline();

    void setupDescriptorBuffer(const DeferredResolveDescriptor& drawInfo);

    void draw(VkCommandBuffer cmd, const DeferredResolveDrawInfo& drawInfo) const;

private:
    ResourceManager& resourceManager;

    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};

    DescriptorBufferSampler resolveDescriptorBuffer;
};
}


#endif //DEFERRED_RESOLVE_H
