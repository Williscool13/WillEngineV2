//
// Created by William on 8/11/2024.
// This class contains structs and classes that help with management of descriptors. THis includes descriptor layouts and buffers.
//

#ifndef VKDESCRIPTORS_H
#define VKDESCRIPTORS_H
#include <vector>
#include <vulkan/vulkan_core.h>


struct DescriptorLayoutBuilder {

    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void add_binding(uint32_t binding, VkDescriptorType type);
    void add_binding(uint32_t binding, VkDescriptorType type, uint32_t count);
    void clear();
    VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};




#endif //VKDESCRIPTORS_H
