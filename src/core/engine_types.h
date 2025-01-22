//
// Created by William on 2025-01-22.
//

#ifndef ENGINE_TYPES_H
#define ENGINE_TYPES_H
#include <vulkan/vulkan_core.h>

namespace will_engine
{
struct EngineStats
{
    float physicsTime{};
    float gameTime{};
    float renderTime{};
    float totalTime{};
};

struct FrameData
{
    VkCommandPool _commandPool;
    VkCommandBuffer _mainCommandBuffer;
    VkSemaphore _swapchainSemaphore, _renderSemaphore;
    VkFence _renderFence;
};
}


#endif //ENGINE_TYPES_H
