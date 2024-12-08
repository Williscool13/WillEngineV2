//
// Created by William on 2024-12-07.
//

#include "render_system.h"

RenderSystem::RenderSystem(VulkanContext& context) : vulkanContext(context)
{

}

RenderSystem::~RenderSystem() {
    cleanup();
}

void RenderSystem::cleanup() {}
