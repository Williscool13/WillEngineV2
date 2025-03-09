//
// Created by William on 2024-12-15.
//

#ifndef IMGUI_WRAPPER_H
#define IMGUI_WRAPPER_H

#include <src/renderer/vulkan_context.h>

#include "src/core/game_object/hierarchical.h"
#include "src/core/scene/map.h"

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

    static void handleInput(const SDL_Event& e);

    void selectMap(Map* newMap);

    void imguiInterface(Engine* engine);

    void drawSceneGraph(Engine* engine);

    void displayGameObject(Engine* engine, IHierarchical* obj, int32_t depth);

    void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView, VkExtent2D swapchainExtent);

private:
    static void indent(IHierarchical* obj);

    static void undent(IHierarchical* obj);

    static void moveObject(const IHierarchical* obj, int diff);

    /**
     * Gets the index of the specified gameobject in the vector. Returns -1 if not found. O(n) complexity
     * @param obj
     * @param vector
     * @return
     */
    static int getIndexInVector(const IHierarchical* obj, const std::vector<IHierarchical*>& vector);

private:
    const VulkanContext& context;

    VkDescriptorPool imguiPool{VK_NULL_HANDLE};

    IHierarchical* selectedItem{nullptr};
    Map* selectedMap{nullptr};

    NoiseSettings terrainProperties{};
    uint32_t terrainSeed{13};
};
}

#endif //IMGUI_WRAPPER_H
