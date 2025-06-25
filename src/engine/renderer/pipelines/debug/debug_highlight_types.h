//
// Created by William on 2025-05-09.
//

#ifndef DEBUG_HIGHLIGHT_TYPES_H
#define DEBUG_HIGHLIGHT_TYPES_H

#include <span>
#include <glm/glm.hpp>

#include "engine/renderer/assets/render_object/render_object_types.h"
#include "engine/renderer/renderer_constants.h"


namespace will_engine::renderer
{
class IRenderable;

struct DebugHighlightDrawPushConstant
{
    glm::mat4 modelMatrix{1.0f};
    glm::vec4 color{1.0f, 0.647f, 0.0f, 1.0f};
};

struct HighlightData
{
    VkBuffer vertexBuffer{VK_NULL_HANDLE};
    VkBuffer indexBuffer{VK_NULL_HANDLE};
    std::span<const Primitive> primitives;

    glm::mat4 modelMatrix{1.0f};
};

struct DebugHighlighterDrawInfo
{
    IRenderable* highlightTarget{nullptr};
    VkExtent2D extents{DEFAULT_RENDER_EXTENT_2D};
    VkImageView depthStencilTarget{VK_NULL_HANDLE};
    VkDescriptorBufferBindingInfoEXT sceneDataBinding{};
    VkDeviceSize sceneDataOffset{0};
};
}

#endif //DEBUG_HIGHLIGHT_TYPES_H
