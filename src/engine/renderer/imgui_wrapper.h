//
// Created by William on 2024-12-15.
//

#ifndef IMGUI_WRAPPER_H
#define IMGUI_WRAPPER_H

#include "engine/renderer/vulkan_context.h"
#include "engine/core/game_object/hierarchical.h"
#include "engine/core/scene/map.h"
#include "terrain/terrain_constants.h"

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


    /**
     *
     * @param engine
     * @param obj
     * @param depth
     * @return indicates whether an action has been taken to invalidate the whole view, in which case drawing should be halted
     */
    bool displayGameObject(Engine* engine, IHierarchical* obj, int32_t depth);

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
    static void selectItem(Engine* engine, IHierarchical* hierarchical);

    static void deselectItem(Engine* engine);

private:
    const VulkanContext& context;

    VkDescriptorPool imguiPool{VK_NULL_HANDLE};


    Map* selectedMap{nullptr};

    uint32_t selectedRenderObjectId = 0;
    std::shared_ptr<renderer::TextureResource> currentlySelectedTexture;

    int32_t shadowMapDebug{0};

    NoiseSettings terrainGenerationSettings{};
    uint32_t terrainSeed{13};
    terrain::TerrainConfig terrainConfig{};

    terrain::TerrainProperties terrainProperties{};
    std::array<uint32_t, terrain::MAX_TERRAIN_TEXTURE_COUNT> terrainTextures;

    VkDescriptorSet currentlySelectedTextureImguiId{VK_NULL_HANDLE};

    bool showGtaoDebugPreview = false;
    VkDescriptorSet aoDebugTextureImguiId{VK_NULL_HANDLE};

    VkDescriptorSet contactShadowsOutputImguiId{VK_NULL_HANDLE};
};
}

#endif //IMGUI_WRAPPER_H
