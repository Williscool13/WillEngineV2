//
// Created by William on 2025-01-22.
//

#ifndef DESCRIPTOR_BUFFER_UNIFORM_H
#define DESCRIPTOR_BUFFER_UNIFORM_H
#include "descriptor_buffer.h"
#include "descriptor_buffer_types.h"

namespace will_engine
{
using will_engine::DescriptorUniformData;

class DescriptorBufferUniform final : public DescriptorBuffer
{
public:
    DescriptorBufferUniform() = default;

    /**
     *
     * @param context
     * @param descriptorSetLayout
     * @param maxObjectCount
     */
    DescriptorBufferUniform(const VulkanContext& context, VkDescriptorSetLayout descriptorSetLayout, int32_t maxObjectCount = 10);

    /**
     * Allocates a descriptor set instance to a free index in the descriptor buffer. The vector passed to this should be equal in layout to
     * the descriptor set layout this descriptor buffer was initialized with.
     * @param device the device the uniform buffer is created in and this descriptor buffer was initialized with
     * @param uniformBuffers the uniform buffer and their corresponding total sizes to insert to the descriptor buffer
     * @param index
     * @return the index of the allocated descriptor set. Store and use when binding during draw call.
     */
    int32_t setupData(VkDevice device, const std::vector<DescriptorUniformData>& uniformBuffers, int32_t index = -1);

    VkBufferUsageFlagBits getBufferUsageFlags() const override;
};
}



#endif //DESCRIPTOR_BUFFER_UNIFORM_H
