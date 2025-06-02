//
// Created by William on 2025-05-27.
//

#include "image_ktx.h"

#include <fmt/format.h>

#include "engine/renderer/resource_manager.h"
#include "engine/renderer/vk_helpers.h"

namespace will_engine::renderer
{
ImageKtx::ImageKtx(ResourceManager* resourceManager, ktxTexture* ktxTexture)
    : ImageResource(resourceManager)
{
    const ktx_error_code_e result = ktxTexture_VkUploadEx(ktxTexture, manager->getKtxVulkanDeviceInfo(), &texture, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (result != KTX_SUCCESS) {
        const char* errorCode = ktxErrorString(result);
        fmt::print("Error when uploading ktx texture (code: {})", errorCode);
        return;
    }

    image = texture.image;
    imageLayout = texture.imageLayout;
    imageFormat = texture.imageFormat;
    imageExtent = {texture.width, texture.height, texture.depth};
    mipLevels = texture.levelCount;

    VkImageAspectFlags aspectFlag;
    if (imageFormat == VK_FORMAT_D32_SFLOAT) {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    } else if (imageFormat == VK_FORMAT_D24_UNORM_S8_UINT) {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    } else {
        aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    VkImageViewCreateInfo viewInfo = vk_helpers::imageviewCreateInfo(texture.imageFormat, VK_NULL_HANDLE, aspectFlag);
    viewInfo.viewType = texture.viewType;
    viewInfo.subresourceRange.layerCount = texture.layerCount;
    viewInfo.subresourceRange.levelCount = texture.levelCount;
    viewInfo.image = image;
    VK_CHECK(vkCreateImageView(resourceManager->getDevice(), &viewInfo, nullptr, &imageView));
}

ImageKtx::~ImageKtx()
{
    ktxVulkanTexture_Destruct(&texture, manager->getKtxVulkanDeviceInfo()->device, nullptr);

    if (imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(manager->getDevice(), imageView, nullptr);
        imageView = VK_NULL_HANDLE;
    }
}
}
