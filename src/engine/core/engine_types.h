//
// Created by William on 2025-01-22.
//

#ifndef ENGINE_TYPES_H
#define ENGINE_TYPES_H
#include <filesystem>
#include <vulkan/vulkan_core.h>

namespace will_engine
{
struct FrameData
{
    VkCommandPool _commandPool;
    VkCommandBuffer _mainCommandBuffer;
    VkSemaphore _swapchainSemaphore, _renderSemaphore;
    VkFence _renderFence;
};

struct EditorSettings
{
    bool bSaveSettingsOnExit{true};
    bool bSaveMapOnExit{true};
};

struct EngineSettings
{
    std::filesystem::path defaultMapToLoad{};
};
}


#endif //ENGINE_TYPES_H
