//
// Created by William on 2025-01-22.
//

#ifndef ENGINE_TYPES_H
#define ENGINE_TYPES_H
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

struct EngineSettings
{
    bool saveOnExit{true};
};
}


#endif //ENGINE_TYPES_H
