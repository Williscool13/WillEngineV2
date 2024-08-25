//
// Created by William on 8/11/2024.
//

#ifndef VKTYPES_H
#define VKTYPES_H

#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>


class VkTypes {

};
struct CubemapImageView {
    VkImageView imageView;
    VkExtent3D imageExtent;
    float roughness;
    int descriptorBufferIndex;
};

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

struct AllocatedCubemap {
    AllocatedImage allocatedImage;
    std::vector<CubemapImageView> cubemapImageViews;
    int mipLevels; //should be equal to cubemapImageViews.size()

};




#endif //VKTYPES_H
