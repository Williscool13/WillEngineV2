//
// Created by William on 2024-12-13.
//

#ifndef IMMEDIATE_SUBMITTER_H
#define IMMEDIATE_SUBMITTER_H

#include <functional>
#include <vulkan/vulkan_core.h>

#include "vk_helpers.h"
#include "volk/volk.h"
#include "vulkan_context.h"

class ImmediateSubmitter
{
public:
    explicit ImmediateSubmitter(const VulkanContext& context) : context(context)
    {
        const VkFenceCreateInfo fenceInfo = vk_helpers::fenceCreateInfo();
        VK_CHECK(vkCreateFence(context.device, &fenceInfo, nullptr, &immFence));

        const VkCommandPoolCreateInfo poolInfo = vk_helpers::commandPoolCreateInfo(context.graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        VK_CHECK(vkCreateCommandPool(context.device, &poolInfo, nullptr, &immCommandPool));

        const VkCommandBufferAllocateInfo allocInfo = vk_helpers::commandBufferAllocateInfo(immCommandPool, 1);
        VK_CHECK(vkAllocateCommandBuffers(context.device, &allocInfo, &immCommandBuffer));
    }

    ~ImmediateSubmitter()
    {
        vkDestroyCommandPool(context.device, immCommandPool, nullptr);
        vkDestroyFence(context.device, immFence, nullptr);
    }

    void submit(const std::function<void(VkCommandBuffer cmd)>& function) const
    {
        VK_CHECK(vkResetFences(context.device, 1, &immFence));
        VK_CHECK(vkResetCommandBuffer(immCommandBuffer, 0));

        const auto cmd = immCommandBuffer;
        const VkCommandBufferBeginInfo cmdBeginInfo = vk_helpers::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
        function(cmd);
        VK_CHECK(vkEndCommandBuffer(cmd));

        VkCommandBufferSubmitInfo cmdSubmitInfo = vk_helpers::commandBufferSubmitInfo(cmd);
        const VkSubmitInfo2 submitInfo = vk_helpers::submitInfo(&cmdSubmitInfo, nullptr, nullptr);

        VK_CHECK(vkQueueSubmit2(context.graphicsQueue, 1, &submitInfo, immFence));
        VK_CHECK(vkWaitForFences(context.device, 1, &immFence, true, 1000000000));
    }

private:
    const VulkanContext& context;
    VkFence immFence{VK_NULL_HANDLE};
    VkCommandPool immCommandPool{VK_NULL_HANDLE};
    VkCommandBuffer immCommandBuffer{VK_NULL_HANDLE};
};


#endif //IMMEDIATE_SUBMITTER_H
