//
// Created by William on 2024-12-15.
//

#ifndef IMGUI_WRAPPER_H
#define IMGUI_WRAPPER_H

#include <src/renderer/vulkan_context.h>

#include "src/core/game_object/hierarchical.h"
#include "src/core/scene/scene.h"

namespace will_engine
{
class Engine;

struct ImguiWrapperInfo
{
    SDL_Window* window;
    VkFormat swapchainImageFormat;
};

/**
 * All dear imgui vulkan/SDL2 integration. Mostly copied from imgui's samples
 */
class ImguiWrapper
{
public:
    friend class Engine;

    ImguiWrapper(const VulkanContext& context, const ImguiWrapperInfo& imguiWrapperInfo);

    ~ImguiWrapper();

    void handleInput(const SDL_Event& e);

    void imguiInterface(Engine* engine);

    static void drawSceneGraph(Engine* engine, const Scene* scene);

    static void displayGameObject(Engine* engine, const Scene* scene, IHierarchical* obj, int32_t depth);

    void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView, VkExtent2D swapchainExtent);

private:
    const VulkanContext& context;

    VkDescriptorPool imguiPool{VK_NULL_HANDLE};
};
}

#endif //IMGUI_WRAPPER_H
