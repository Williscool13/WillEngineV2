//
// Created by William on 2025-03-15.
//

#ifndef TEXTURE_H
#define TEXTURE_H
#include <filesystem>

#include "src/renderer/resource_manager.h"
#include "src/renderer/vk_types.h"


namespace will_engine
{
struct TextureProperties
{
    bool mipmapped{true};
};

class Texture
{
public:
    Texture() = delete;

    Texture(ResourceManager& resourceManager, uint32_t textureId, const std::filesystem::path& willTexturePath, const std::filesystem::path& texturePath, TextureProperties textureProperties);

    ~Texture();

    void load();

    void unload() const;

public:
    AllocatedImage getTextureResource() const { return texture; }

    uint32_t getId() const { return textureId; }

    const std::string& getName() { return name; }

    const std::filesystem::path& getTexturePath() { return texturePath; }

    const std::filesystem::path& getWillTexturePath() { return willTexturePath; }

private:
    AllocatedImage texture;

private:
    uint32_t textureId;
    std::filesystem::path willTexturePath;
    std::filesystem::path texturePath;
    std::string name;

    TextureProperties properties;

    bool bIsLoaded{false};

private:
    ResourceManager& resourceManager;
};
}


#endif //TEXTURE_H
