//
// Created by William on 2025-03-20.
//

#ifndef TEXTURE_RESOURCE_H
#define TEXTURE_RESOURCE_H

#include "texture_types.h"
#include "engine/renderer/resource_manager.h"
#include "engine/renderer/resources/image_resource.h"
#include "engine/renderer/resources/resources_fwd.h"


namespace will_engine::renderer
{
class TextureResource
{
public:
    TextureResource(ResourceManager& resourceManager, const std::filesystem::path& texturePath, uint32_t textureId, TextureProperties properties);

    ~TextureResource();

    uint32_t getId() const { return textureId; }

    VkImageView getImageView() const { return image->imageView; }
    VkExtent3D getExtent() const { return image->imageExtent; }

private:
    ResourceManager& resourceManager;
    ImageResourcePtr image;

    uint32_t textureId;
};
} // will_engine

#endif //TEXTURE_RESOURCE_H
