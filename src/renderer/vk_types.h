//
// Created by William on 8/11/2024.
//

#ifndef VKTYPES_H
#define VKTYPES_H

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

struct AllocatedImage {
    VkImage image {VK_NULL_HANDLE};
    VkImageView imageView{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
    VkExtent3D imageExtent{};
    VkFormat imageFormat{};
};

struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};




#endif //VKTYPES_H
