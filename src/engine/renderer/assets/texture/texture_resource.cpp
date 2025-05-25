//
// Created by William on 2025-03-20.
//

#include "texture_resource.h"

#include <stb/stb_image.h>

#include "texture.h"

namespace will_engine::renderer
{
TextureResource::TextureResource(ResourceManager& resourceManager, const std::filesystem::path& texturePath, const uint32_t textureId, const TextureProperties properties) : resourceManager(resourceManager), textureId(textureId)
{
    if (!exists(texturePath)) {
        fmt::print("Error: Texture file not found: {}\n", texturePath.string());
        return;
    }

    int32_t width, height, channels;
    unsigned char* data = stbi_load(texturePath.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!data) {
        fmt::print("Failed to load texture: {}\n", texturePath.string());
        fmt::print("STB Error: {}\n", stbi_failure_reason());
        return;
    }

    const VkExtent3D imageExtent{
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
        1
    };

    const size_t imageSize = width * height * 4;
    image = resourceManager.createImageFromData(
        data,
        imageSize,
        imageExtent,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        properties.mipmapped
    );

    stbi_image_free(data);
}

TextureResource::~TextureResource()
{
    resourceManager.destroyResource(std::move(image));
}
} // will_engine
