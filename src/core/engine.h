//
// Created by William on 8/11/2024.
//

#ifndef ENGINE_H
#define ENGINE_H

#include "engine_types.h"
#include "src/renderer/imgui_wrapper.h"
#include "src/renderer/immediate_submitter.h"
#include "src/renderer/renderer_constants.h"
#include "src/renderer/pipelines/basic_compute/basic_compute_pipeline.h"
#include "src/renderer/pipelines/basic_render/basic_render_pipeline.h"
#include "src/renderer/descriptor_buffer/descriptor_buffer_uniform.h"
#include "src/renderer/pipelines/deferred_mrt/deferred_mrt.h"

namespace will_engine
{
class RenderObject;
class GameObject;

namespace physics
{
    class Physics;
}

class Engine
{
public:
    void init();

    void run();

    void draw();

    /**
     * Cleans up vulkan resources when application has exited. Destroys resources in opposite order of initialization
     * \n Resources -> Command Pool (implicit destroy C. Buffers) -> Swapchain -> Surface -> Device -> Instance -> Window
     */
    void cleanup();

private:
    VkExtent2D windowExtent{1700, 900};
    SDL_Window* window{nullptr};

    VulkanContext* context{nullptr};
    ImmediateSubmitter* immediate = nullptr;
    ResourceManager* resourceManager = nullptr;
    physics::Physics* physics = nullptr;
    ImguiWrapper* imguiWrapper = nullptr;

    EngineStats stats{};

private: // Rendering
    int32_t frameNumber{0};
    FrameData frames[FRAME_OVERLAP]{};
    int32_t getCurrentFrameOverlap() const { return frameNumber % FRAME_OVERLAP; }
    FrameData& getCurrentFrame() { return frames[getCurrentFrameOverlap()]; }

    bool bStopRendering{false};
    bool bResizeRequested{false};


    AllocatedImage drawImage{};
    AllocatedImage depthImage{};

    void createDrawResources();

private: // Scene Data
    DescriptorBufferUniform sceneDataDescriptorBuffer;
    AllocatedBuffer sceneDataBuffers[FRAME_OVERLAP]{};

    RenderObject* cube{nullptr};

    GameObject* test{nullptr};

private: // Pipelines
    basic_compute::BasicComputePipeline* computePipeline{nullptr};
    basic_render::BasicRenderPipeline* renderPipeline{nullptr};
    deferred_mrt::DeferredMrtPipeline* deferredMrtPipeline{nullptr};

private: // Render Targets
    /**
     * 8.8.8 Normals - 8 unused
     */
    AllocatedImage normalRenderTarget{};
    /**
     * 8.8.8 RGB Albedo - 8 unused
     */
    AllocatedImage albedoRenderTarget{};
    /**
     * 8 Metallic, 8 Roughness, 8 emission (unused), 8 Unused
     */
    AllocatedImage pbrRenderTarget{};
    /**
     * 16 X and 16 Y
     */
    AllocatedImage velocityRenderTarget{};


private: // Swapchain
    VkSwapchainKHR swapchain{};
    VkFormat swapchainImageFormat{};
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkExtent2D swapchainExtent{};

    void createSwapchain(uint32_t width, uint32_t height);

    void resizeSwapchain();

public:
    friend void ImguiWrapper::imguiInterface(const Engine* engine);
};
}


#endif //ENGINE_H
