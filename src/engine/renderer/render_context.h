//
// Created by William on 2025-06-20.
//

#ifndef RENDER_CONTEXT_H
#define RENDER_CONTEXT_H
#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>

#include "renderer_constants.h"
#include "engine/core/events/event_dispatcher.h"

namespace will_engine::renderer
{
struct ResolutionChangedEvent {
    VkExtent2D oldExtent;
    VkExtent2D newExtent;
    float oldRenderScale;
    float newRenderScale;
};

class RenderContext
{
public:
    struct PendingChanges
    {
        bool windowResizePending = false;
        bool renderScaleChangePending = false;
        VkExtent2D newWindowExtent = {0, 0};
        float newRenderScale = 1.0f;

        bool hasPendingChanges() const
        {
            return windowResizePending || renderScaleChangePending;
        }

        void clear()
        {
            windowResizePending = false;
            renderScaleChangePending = false;
        }
    };

public:
    RenderContext(VkExtent2D windowExtent, float renderScale);

    /**
     * Dispatched when `applyPendingChanges` is called. This should only execute when there are no FIFs (vkDeviceWaitIdle).
     */
    EventDispatcher<ResolutionChangedEvent> resolutionChangedEvent;
    EventDispatcher<ResolutionChangedEvent> postResolutionChangedEvent;

public:
    VkExtent2D windowExtent{1920, 1080};
    VkExtent2D renderExtent{1920, 1080};

    PendingChanges pending{};

    float renderScale = 1.0f;
    /**
     * So we don't have to check if > 0 in advance frame
     */
    uint32_t frameNumber = 1;
    uint32_t currentFrameIndex = 0;
    uint32_t previousFrameIndex = 0;

    void requestWindowResize(const uint32_t width, const uint32_t height)
    {
        pending.newWindowExtent = {width, height};
        pending.windowResizePending = true;
    }

    void requestRenderScale(float scale)
    {
        scale = glm::clamp(scale, 0.1f, 2.0f);
        pending.newRenderScale = scale;
        pending.renderScaleChangePending = true;
    }

    bool hasPendingChanges() const;

    bool applyPendingChanges();

    void advanceFrame()
    {
        frameNumber++;
        currentFrameIndex = frameNumber % FRAME_OVERLAP;
        previousFrameIndex = (frameNumber - 1) % FRAME_OVERLAP;
    }
};
}

#endif //RENDER_CONTEXT_H
