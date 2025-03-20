//
// Created by William on 2025-03-20.
//

#ifndef TEXTURE_RESOURCE_H
#define TEXTURE_RESOURCE_H

#include "texture_types.h"
#include "src/renderer/resource_manager.h"
#include "src/renderer/vk_types.h"

namespace will_engine
{
class TextureResource
{
public:
    TextureResource(ResourceManager& resourceManager, const std::filesystem::path& texturePath, uint32_t textureId, TextureProperties properties);

    ~TextureResource();

    uint32_t getId() const { return textureId; }

    const AllocatedImage& getTexture() const { return image; }

private:
    ResourceManager& resourceManager;
    AllocatedImage image;

    uint32_t textureId;
};
} // will_engine

#endif //TEXTURE_RESOURCE_H
