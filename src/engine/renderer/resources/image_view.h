//
// Created by William on 2025-05-25.
//

#ifndef IMAGE_VIEW_H
#define IMAGE_VIEW_H
#include <cassert>
#include <utility>
#include <vulkan/vulkan_core.h>

#include "vulkan_resource.h"
#include "engine/renderer/vulkan_context.h"

namespace will_engine::renderer
{
/**
* A wrapper for a VkImageView.
* \n Note that it is the responsibility of the programmer to ensure that the VkImage that this ImageView references is still alive when this is accessed.
*/
class ImageView : public VulkanResource
{
public:
    VkImageView imageView{VK_NULL_HANDLE};

    ImageView() = default;

    ~ImageView() override
    {
        assert(m_destroyed && "Resource not properly destroyed before destructor!");
    }

    void release(VulkanContext& context) override;

    bool isValid() const override;

    static ImageView create(VkDevice device, const VkImageViewCreateInfo& createInfo);

    // No copying
    ImageView(const ImageView&) = delete;

    ImageView& operator=(const ImageView&) = delete;

    // Move constructor
    ImageView(ImageView&& other) noexcept
        : imageView(std::exchange(other.imageView, VK_NULL_HANDLE))
          , m_destroyed(other.m_destroyed)
    {
        other.m_destroyed = true;
    }

    ImageView& operator=(ImageView&& other) noexcept
    {
        if (this != &other) {
            imageView = std::exchange(other.imageView, VK_NULL_HANDLE);
            m_destroyed = other.m_destroyed;
            other.m_destroyed = true;
        }
        return *this;
    }

private:
    bool m_destroyed{true};
};
}

#endif //IMAGE_VIEW_H
