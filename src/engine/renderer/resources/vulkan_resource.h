//
// Created by William on 2025-05-24.
//

#ifndef VULKAN_RESOURCE_H
#define VULKAN_RESOURCE_H


namespace will_engine
{
class VulkanContext;
}

namespace will_engine::renderer
{

class VulkanResource {
public:
    VulkanResource() = default;
    virtual ~VulkanResource() = default;
    virtual void release(VulkanContext& context) = 0;
    virtual bool isValid() const = 0;

    VulkanResource(const VulkanResource&) = delete;
    VulkanResource& operator=(const VulkanResource&) = delete;
};
}

#endif //VULKAN_RESOURCE_H
