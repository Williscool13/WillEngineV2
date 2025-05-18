//
// Created by William on 2025-01-22.
//

#ifndef DESCRIPTOR_BUFFER_TYPES_H
#define DESCRIPTOR_BUFFER_TYPES_H
#include <stdexcept>
#include <vulkan/vulkan_core.h>

#include "engine/renderer/vk_types.h"

namespace will_engine
{
struct DescriptorImageData
{
    VkDescriptorType type{VK_DESCRIPTOR_TYPE_SAMPLER};
    VkDescriptorImageInfo imageInfo{};
    bool bIsPadding{false};
};

struct DescriptorUniformData
{
    AllocatedBuffer uniformBuffer{};
    size_t allocSize{};
};


class DescriptorBufferException final : public std::runtime_error
{
public:
    explicit DescriptorBufferException(const std::string& message) : std::runtime_error(message) {}
};

}


#endif //DESCRIPTOR_BUFFER_TYPES_H
