//
// Created by William on 2025-03-15.
//

#ifndef TEXTURE_H
#define TEXTURE_H
#include <filesystem>

#include "texture_resource.h"
#include "texture_types.h"
#include "src/renderer/resource_manager.h"

namespace will_engine
{
class Texture
{
public:
    Texture() = delete;

    Texture(ResourceManager& resourceManager, uint32_t textureId, const std::filesystem::path& willTexturePath, const std::filesystem::path& texturePath, TextureProperties textureProperties);

    ~Texture();

    std::shared_ptr<TextureResource> getTextureResource();

    bool isTextureResourceLoaded() const { return !textureResource.expired(); }

public:
    uint32_t getId() const { return textureId; }

    const std::string& getName() const { return name; }

    const std::filesystem::path& getTexturePath() const { return texturePath; }

    const std::filesystem::path& getWillTexturePath() const { return willTexturePath; }

private:
    std::weak_ptr<TextureResource> textureResource;

private:
    uint32_t textureId;
    std::filesystem::path willTexturePath;
    std::filesystem::path texturePath;
    std::string name;

    TextureProperties properties;

private:
    ResourceManager& resourceManager;
};
}


#endif //TEXTURE_H
