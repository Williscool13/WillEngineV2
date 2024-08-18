//
// Created by William on 8/11/2024.
//

#ifndef VKHELPERS_H
#define VKHELPERS_H

#include <cmath>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vk_enum_string_helper.h>
#include "volk.h"

#include "fmt/format.h"
#include "../renderer/vk_types.h"

namespace vk_helpers
{
    VkImageCreateInfo imageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);

    VkImageViewCreateInfo imageviewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);

    VkImageSubresourceRange imageSubresourceRange(VkImageAspectFlags aspectMask);


    VkCommandPoolCreateInfo commandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);

    VkCommandBufferAllocateInfo commandBufferAllocateInfo(VkCommandPool pool, uint32_t count = 1);

    VkCommandBufferBeginInfo commandBufferBeginInfo(VkCommandBufferUsageFlags flags = 0);

    VkCommandBufferSubmitInfo commandBufferSubmitInfo(VkCommandBuffer cmd);


    VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags = 0);

    VkSemaphoreCreateInfo semaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0);

    VkSemaphoreSubmitInfo semaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);

    VkRenderingAttachmentInfo attachmentInfo(VkImageView view, VkClearValue* clear, VkImageLayout layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/);

    VkRenderingInfo renderingInfo(VkExtent2D renderExtent, VkRenderingAttachmentInfo* colorAttachment, VkRenderingAttachmentInfo* depthAttachment);

    VkSubmitInfo2 submitInfo(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo);
    VkPresentInfoKHR presentInfo();


    VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo(VkDeviceAddress bufferAddress, VkBufferUsageFlagBits usageFlags);

    /**
     * Returns the Buffer Device address of the specified buffer
     * @param device The device the buffer was created with
     * @param buffer The buffer whose address will be returned
     * @return the address of the buffer
     */
    VkDeviceAddress getDeviceAddress(VkDevice device, VkBuffer buffer);

    /**
     * Returns the final aligned size based on the alignment given
     * @param value value to align
     * @param alignment value to align to
     * @return
     */
    VkDeviceSize getAlignedSize(VkDeviceSize value, VkDeviceSize alignment);


    void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout targetLayout);

    void copyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);

    void generateMipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);
}


#define VK_CHECK(x)                                                          \
    do {                                                                     \
        VkResult err = x;                                                    \
        if (err) {                                                           \
            fmt::print("Detected Vulkan error: {}\n", string_VkResult(err)); \
            abort();                                                         \
        }                                                                    \
    } while (0)

#endif //VKHELPERS_H
