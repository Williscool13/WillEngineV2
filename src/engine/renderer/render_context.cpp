//
// Created by William on 2025-06-20.
//

#include <glm/glm.hpp>
#include <fmt/format.h>

#include "render_context.h"

namespace will_engine::renderer
{
RenderContext::RenderContext(const VkExtent2D windowExtent, float renderScale)
    : windowExtent(windowExtent), renderScale(renderScale)
{
    pending.newWindowExtent = windowExtent;
    pending.windowResizePending = true;
    renderScale = glm::clamp(renderScale, 0.1f, 2.0f);
    pending.newRenderScale = renderScale;
    pending.renderScaleChangePending = true;

    applyPendingChanges();
}

bool RenderContext::applyPendingChanges()
{
    if (!pending.hasPendingChanges()) return false;

    if (pending.windowResizePending) {
        windowExtent = pending.newWindowExtent;
        fmt::print("Window resize applied.\n");
    }

    if (pending.renderScaleChangePending) {
        renderScale = pending.newRenderScale;
        fmt::print("Render scale change applied.\n");
    }

    renderExtent = {
        static_cast<uint32_t>(windowExtent.width * renderScale),
        static_cast<uint32_t>(windowExtent.height * renderScale)
    };

    pending.clear();
    return true;
}
}
