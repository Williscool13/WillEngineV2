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
#include "src/renderer/vulkan/descriptor_buffer/descriptor_buffer_uniform.h"


using will_engine::EngineStats;
using will_engine::FrameData;
using basic_compute::BasicComputePipeline;


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
    // Main
    int frameNumber{0};
    FrameData frames[FRAME_OVERLAP]{};
    FrameData& getCurrentFrame() { return frames[frameNumber % FRAME_OVERLAP]; };

    bool bStopRendering{false};
    bool bResizeRequested{false};

    friend void ImguiWrapper::imguiInterface(Engine* engine);

    const VkFormat drawImageFormat{VK_FORMAT_R16G16B16A16_SFLOAT};
    const VkFormat depthImageFormat{VK_FORMAT_D32_SFLOAT};
    const VkFormat velocityImageFormat{VK_FORMAT_R16G16_SFLOAT};
    const VkFormat normalImageFormat{VK_FORMAT_R16G16B16A16_SNORM}; //VK_FORMAT_R8G8B8A8_SNORM - 8888 is too inaccurate for normals
    const VkFormat albedoImageFormat{VK_FORMAT_R8G8B8A8_UNORM};
    const VkFormat pbrImageFormat{VK_FORMAT_R8G8B8A8_UNORM};

private: // Pipelines
    BasicComputePipeline* computePipeline{nullptr};

    VkDescriptorSetLayout renderImageDescriptorSetLayout;
    VkDescriptorSetLayout renderUniformDescriptorSetLayout;
    DescriptorBufferSampler renderImageDescriptorBuffer;
    DescriptorBufferUniform renderUniformDescriptorBuffer;
    VkPipelineLayout renderPipelineLayout;

    VkPipeline renderPipeline;

private: // Swapchain
    VkSwapchainKHR swapchain{};
    VkFormat swapchainImageFormat{};
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkExtent2D swapchainExtent{};

    void createSwapchain(uint32_t width, uint32_t height);
    void resizeSwapchain();

private: // Draw Images
    AllocatedImage drawImage{};
    AllocatedImage depthImage{};
    const VkExtent2D renderExtent{1920, 1080};
    float renderScale{1.0f};
    float maxRenderScale{1.0f};

    void createDrawResources(uint32_t width, uint32_t height);
};

#endif //ENGINE_H
