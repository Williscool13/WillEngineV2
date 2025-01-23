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
#include "src/renderer/vulkan/descriptor_buffer/descriptor_buffer_uniform.h"


using will_engine::EngineStats;
using will_engine::FrameData;


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
    // Physics* physics = nullptr;
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

    void createDrawResources(uint32_t width, uint32_t height);

private: // Scene Data
    DescriptorBufferUniform sceneDataDescriptorBuffer;
    AllocatedBuffer sceneDataBuffers[FRAME_OVERLAP]{};
    VkDescriptorSetLayout sceneDataLayout{VK_NULL_HANDLE};

private: // Pipelines
    basic_compute::BasicComputePipeline* computePipeline{nullptr};
    basic_render::BasicRenderPipeline* renderPipeline{nullptr};


    const VkFormat drawImageFormat{VK_FORMAT_R16G16B16A16_SFLOAT};
    const VkFormat depthImageFormat{VK_FORMAT_D32_SFLOAT};
    const VkFormat velocityImageFormat{VK_FORMAT_R16G16_SFLOAT};
    const VkFormat normalImageFormat{VK_FORMAT_R16G16B16A16_SNORM}; //VK_FORMAT_R8G8B8A8_SNORM - 8888 is too inaccurate for normals
    const VkFormat albedoImageFormat{VK_FORMAT_R8G8B8A8_UNORM};
    const VkFormat pbrImageFormat{VK_FORMAT_R8G8B8A8_UNORM};

private: // Swapchain
    VkSwapchainKHR swapchain{};
    VkFormat swapchainImageFormat{};
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkExtent2D swapchainExtent{};

    void createSwapchain(uint32_t width, uint32_t height);

    void resizeSwapchain();

    friend void ImguiWrapper::imguiInterface(Engine* engine);
};

#endif //ENGINE_H
