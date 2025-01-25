//
// Created by William on 2024-12-15.
//

#include "imgui_wrapper.h"

#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

#include "src/core/engine.h"
#include "src/core/input.h"
#include "src/core/time.h"
#include "src/util/file.h"

namespace will_engine
{
ImguiWrapper::ImguiWrapper(const VulkanContext& context, const ImguiWrapperInfo& imguiWrapperInfo) : context(context)
{
    // DearImGui implementation, basically copied directly from the Vulkan/SDl2 from DearImGui samples.
    // Because this project uses VOLK, additionally need to load functions.
    // DYNAMIC RENDERING (NOT RENDER PASS)
    constexpr VkDescriptorPoolSize pool_sizes[] =
    {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;

    //VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(context.device, &pool_info, nullptr, &imguiPool));

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls

    ImGui::StyleColorsLight();

    VkInstance instance = context.instance;
    // Need to LoadFunction when using VOLK/using VK_NO_PROTOTYPES
    ImGui_ImplVulkan_LoadFunctions([](const char* function_name, void* vulkan_instance) {
        return vkGetInstanceProcAddr(*(static_cast<VkInstance*>(vulkan_instance)), function_name);
    }, &instance);

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForVulkan(imguiWrapperInfo.window);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = context.instance;
    init_info.PhysicalDevice = context.physicalDevice;
    init_info.Device = context.device;
    init_info.QueueFamily = context.graphicsQueueFamily;
    init_info.Queue = context.graphicsQueue;
    init_info.DescriptorPool = imguiPool;
    init_info.Subpass = 0;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    //dynamic rendering parameters for imgui to use
    init_info.UseDynamicRendering = true;
    init_info.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &imguiWrapperInfo.swapchainImageFormat;


    ImGui_ImplVulkan_Init(&init_info);
    ImGui_ImplVulkan_CreateFontsTexture();
}

ImguiWrapper::~ImguiWrapper()
{
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(context.device, imguiPool, nullptr);
}

void ImguiWrapper::handleInput(const SDL_Event& e)
{
    ImGui_ImplSDL2_ProcessEvent(&e);
}

void ImguiWrapper::imguiInterface(const Engine* engine)
{
    Input& input = Input::Get();
    Time& time = Time::Get();

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    if (ImGui::Begin("Main")) {
        ImGui::Text("Physics Time: %.2f ms", engine->stats.physicsTime);
        ImGui::Text("Game Time: %.2f ms", engine->stats.gameTime);
        ImGui::Text("Render Time: %.2f ms", engine->stats.renderTime);
        ImGui::Text("Frame Time: %.2f ms", engine->stats.totalTime);
        ImGui::Text("Delta Time: %.2f ms", time.getDeltaTime() * 1000.0f);
    }
    ImGui::End();

    if (ImGui::Begin("Save Final Image")) {
        if (ImGui::Button("Save Draw Image")) {
            if (file::getOrCreateDirectory(file::imagesSavePath)) {
                const std::filesystem::path path = file::imagesSavePath / "drawImage.png";
                vk_helpers::saveImageRGBA16SFLOAT(*engine->resourceManager, *engine->immediate, engine->drawImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
                                                  path.string().c_str());
            } else {
                fmt::print(" Failed to find/create image save path directory");
            }
        }

        if (ImGui::Button("Save Normal Render Target")) {
            if (file::getOrCreateDirectory(file::imagesSavePath)) {
                const std::filesystem::path path = file::imagesSavePath / "normalRT.png";
                auto unpackFunc = [](const uint32_t packedColor) {
                    glm::vec4 pixel = glm::unpackSnorm4x8(packedColor);
                    pixel.r = pixel.r * 0.5f + 0.5f;
                    pixel.g = pixel.g * 0.5f + 0.5f;
                    pixel.b = pixel.b * 0.5f + 0.5f;
                    pixel.a = 1.0f;
                    return pixel;
                };

                vk_helpers::savePacked32Bit(*engine->resourceManager, *engine->immediate, engine->normalRenderTarget, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
                                            path.string().c_str(), unpackFunc);
            } else {
                fmt::print(" Failed to save normal render target");
            }
        }
    }
    ImGui::End();

    ImGui::Render();
}


// void ImguiWrapper::imguiInterface(::Engine* engine)
// {

// }

void ImguiWrapper::drawImgui(VkCommandBuffer cmd, const VkImageView targetImageView, const VkExtent2D swapchainExtent)
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = "DearImgui Draw Pass";
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

    const VkRenderingAttachmentInfo colorAttachment = vk_helpers::attachmentInfo(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const VkRenderingInfo renderInfo = vk_helpers::renderingInfo(swapchainExtent, &colorAttachment, nullptr);
    vkCmdBeginRendering(cmd, &renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRendering(cmd);

    vkCmdEndDebugUtilsLabelEXT(cmd);
}
}
