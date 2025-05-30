//
// Created by William on 2025-01-22.

#ifndef DESCRIPTOR_BUFFER_SAMPLER_H
#define DESCRIPTOR_BUFFER_SAMPLER_H
#include <span>

#include "descriptor_buffer_types.h"
#include "descriptor_buffer.h"

namespace will_engine::renderer
{
/**
 * A descriptor buffer specifically for images/sampler images, as textures require additional descriptor binding details and are unable to be fully buffer based like Uniforms
 */
struct  DescriptorBufferSampler final : DescriptorBuffer
{
    DescriptorBufferSampler() = delete;

    DescriptorBufferSampler(ResourceManager* resourceManager, VkDescriptorSetLayout descriptorSetLayout, int32_t maxObjectCount = 10);

    /**
     * Allocates a descriptor set instance to a free index in the descriptor buffer. The vector passed to this should be equal in layout to
     * the descriptor set layout this descriptor buffer was initialized with.
     * @param imageBuffers the image/sampler data and how many times they should be registered (useful for padding)
     * @param index optional. If specified, will allocate/overwrite the descriptor set in that index of the descriptor buffer
     * @return the index of the allocated descriptor set. Store and use when binding during draw call.
     */
    int32_t setupData(std::span<DescriptorImageData> imageBuffers, int32_t index = -1);

    VkBufferUsageFlagBits getBufferUsageFlags() const override;
};
}


#endif //DESCRIPTOR_BUFFER_SAMPLER_H
