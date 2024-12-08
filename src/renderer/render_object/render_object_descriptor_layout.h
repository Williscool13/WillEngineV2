//
// Created by William on 2024-12-08.
//

#ifndef RENDER_OBJECT_DESCRIPTOR_LAYOUT_H
#define RENDER_OBJECT_DESCRIPTOR_LAYOUT_H

#include <volk.h>


class VulkanContext;

class RenderObjectDescriptorLayout {
public:
    explicit RenderObjectDescriptorLayout(VulkanContext& context) : context(context)
    {
        createLayouts();
    }

    ~RenderObjectDescriptorLayout()
    {
        cleanup();
    }

    [[nodiscard]] VkDescriptorSetLayout getAddressesLayout() const { return addressesLayout; }
    [[nodiscard]] VkDescriptorSetLayout getTexturesLayout() const { return texturesLayout; }

private:
    void createLayouts();

    void cleanup() const;

    VulkanContext& context;

    /**
     * Material and Instance Buffer Addresses
     */
    VkDescriptorSetLayout addressesLayout{VK_NULL_HANDLE};
    /**
     * Sampler and Image Arrays
     */
    VkDescriptorSetLayout texturesLayout{VK_NULL_HANDLE};
};



#endif //RENDER_OBJECT_DESCRIPTOR_LAYOUT_H
