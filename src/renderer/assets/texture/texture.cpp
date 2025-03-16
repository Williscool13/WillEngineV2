//
// Created by William on 2025-03-15.
//

#include "texture.h"

#include <stb/stb_image.h>


will_engine::Texture::Texture(ResourceManager& resourceManager, const uint32_t textureId, const std::filesystem::path& willTexturePath, const std::filesystem::path& texturePath,
                              const TextureProperties textureProperties)
    : textureId(textureId), willTexturePath(willTexturePath), texturePath(texturePath), properties(textureProperties), resourceManager(resourceManager)
{}

will_engine::Texture::~Texture()
{}

void will_engine::Texture::load()
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
    texture = resourceManager.createImage(
        data,
        imageSize,
        imageExtent,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        properties.mipmapped
    );

    stbi_image_free(data);
}

void will_engine::Texture::unload() const
{
    resourceManager.destroyImage(texture);
}
