//
// Created by William on 2025-01-22.
//

#ifndef DESCRIPTOR_BUFFER_UNIFORM_H
#define DESCRIPTOR_BUFFER_UNIFORM_H
#include <span>

#include "descriptor_buffer.h"
#include "descriptor_buffer_types.h"

namespace will_engine::renderer
{
using will_engine::DescriptorUniformData;

struct DescriptorBufferUniform final : DescriptorBuffer
{
    DescriptorBufferUniform() = delete;

    DescriptorBufferUniform(ResourceManager* resourceManager, VkDescriptorSetLayout descriptorSetLayout, int32_t maxObjectCount = 10);

    /**
     * Allocates a descriptor set instance to a free index in the descriptor buffer. The vector passed to this should be equal in layout to
     * the descriptor set layout this descriptor buffer was initialized with.
     * @param uniformBuffers the uniform buffer and their corresponding total sizes to insert to the descriptor buffer
     * @param index
     * @return the index of the allocated descriptor set. Store and use when binding during draw call.
     */
    int32_t setupData(std::span<DescriptorUniformData> uniformBuffers, int32_t index = -1);

    VkBufferUsageFlagBits getBufferUsageFlags() const override;
};
}



#endif //DESCRIPTOR_BUFFER_UNIFORM_H
