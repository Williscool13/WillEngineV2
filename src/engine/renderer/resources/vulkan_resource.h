//
// Created by William on 2025-05-24.
//

#ifndef VULKAN_RESOURCE_H
#define VULKAN_RESOURCE_H

namespace will_engine::renderer
{
class ResourceManager;

struct VulkanResource
{
protected:
    ResourceManager* manager;

public:
    VulkanResource() = delete;

    explicit VulkanResource(ResourceManager* mgr) : manager(mgr) {}

    virtual ~VulkanResource() = default;
};
}

#endif //VULKAN_RESOURCE_H
