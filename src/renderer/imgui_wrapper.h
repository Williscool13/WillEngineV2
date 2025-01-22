//
// Created by William on 2024-12-15.
//

#ifndef IMGUI_WRAPPER_H
#define IMGUI_WRAPPER_H

#include <src/renderer/vulkan_context.h>

class Engine;

struct ImguiWrapperInfo
{
    SDL_Window* window;
    VkFormat swapchainImageFormat;
};

/**
 * All dear imgui vulkan/SDL2 integration. Mostly copied from imgui's samples
 */
class ImguiWrapper {
    friend class Engine;
    ImguiWrapper(const VulkanContext& context, const ImguiWrapperInfo& imguiWrapperInfo);

    ~ImguiWrapper();

    void handleInput(const SDL_Event& e);

    void imguiInterface(Engine* engine);

    void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView, VkExtent2D swapchainExtent);

private:
    const VulkanContext& context;

    VkDescriptorPool imguiPool{VK_NULL_HANDLE};
};



#endif //IMGUI_WRAPPER_H
