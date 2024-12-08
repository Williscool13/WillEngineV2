//
// Created by William on 2024-12-08.
//

#ifndef SCENE_DESCRIPTOR_LAYOUTS_H
#define SCENE_DESCRIPTOR_LAYOUTS_H

#include <volk.h>

class VulkanContext;

class SceneDescriptorLayouts
{
public:
    explicit SceneDescriptorLayouts(VulkanContext& context) : context(context)
    {
        createLayouts();
    }

    ~SceneDescriptorLayouts()
    {
        cleanup();
    }

    [[nodiscard]] VkDescriptorSetLayout getSceneDataLayout() const { return sceneDataLayout; }

private:
    void createLayouts();

    void cleanup() const;

    VulkanContext& context;
    VkDescriptorSetLayout sceneDataLayout{VK_NULL_HANDLE};
};

#endif //SCENE_DESCRIPTOR_LAYOUTS_H
