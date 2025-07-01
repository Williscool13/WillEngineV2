//
// Created by William on 2025-06-24.
//

#include "render_target.h"

#include "image.h"

namespace will_engine::renderer
{
RenderTarget::RenderTarget(ResourceManager* resourceManager, const VkImageCreateInfo& createInfo, const VmaAllocationCreateInfo& allocInfo,
                           VkImageViewCreateInfo& viewInfo, const VkImageLayout afterClearFormat)
    : Image(resourceManager, createInfo, allocInfo, viewInfo)
{
    this->afterClearFormat = afterClearFormat;
}
}
