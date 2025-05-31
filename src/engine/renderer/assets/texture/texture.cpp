//
// Created by William on 2025-03-15.
//

#include "texture.h"

#include <fmt/format.h>

namespace will_engine::renderer
{
Texture::Texture(ResourceManager& resourceManager, const uint32_t textureId, const std::filesystem::path& willTexturePath, const std::filesystem::path& texturePath,
                              const TextureProperties textureProperties)
    : textureId(textureId), willTexturePath(willTexturePath), texturePath(texturePath), properties(textureProperties), resourceManager(resourceManager)
{}

Texture::~Texture()
{
    if (!textureResource.expired()) {
        fmt::print("Texture: Warning, Texture was destroyed when the texture resource was still in use. This will cause problems");
    }
}

std::shared_ptr<TextureResource> Texture::getTextureResource()
{
    if (textureResource.expired()) {
        auto newTexture = std::make_shared<TextureResource>(resourceManager, texturePath, textureId, properties);
        textureResource = newTexture;
        return newTexture;
    }

    return textureResource.lock();
}

}

