//
// Created by William on 2025-06-24.
//

#ifndef RENDER_TARGET_H
#define RENDER_TARGET_H
#include "image.h"

namespace will_engine::renderer
{

struct RenderTarget : Image
{
    /**
     * The format the render target will be when the format first exits the
    */
    VkImageLayout afterClearFormat;


    RenderTarget(ResourceManager* resourceManager, const VkImageCreateInfo& createInfo, const VmaAllocationCreateInfo& allocInfo, VkImageViewCreateInfo& viewInfo, VkImageLayout afterClearFormat);
};

}

#endif //RENDER_TARGET_H
