//
// Created by William on 2025-03-15.
//

#include "asset_manager.h"

#include "engine/util/file.h"

namespace will_engine::renderer
{
AssetManager::AssetManager(ResourceManager& resourceManager) : resourceManager(resourceManager)
{}

AssetManager::~AssetManager()
{}

void AssetManager::scanForAll()
{
    fmt::print("Scanning for .willmodel and .willtexture files\n");

    scanForRenderObjects();
    scanForTextures();
}

void AssetManager::scanForTextures()
{
    const std::vector<std::filesystem::path> willTextures = file::findWillFiles(relative(std::filesystem::current_path() / "assets"), ".willtexture");
    textures.reserve(willTextures.size());
    for (std::filesystem::path willTexture : willTextures) {
        std::optional<TextureInfo> textureInfo = Serializer::loadWillTexture(willTexture);
        if (textureInfo.has_value()) {
            if (!textures.contains(textureInfo->id)) {
                textures[textureInfo->id] = std::make_unique<Texture>(resourceManager, textureInfo->id, textureInfo->willtexturePath,
                                                                      std::filesystem::path(textureInfo->texturePath),
                                                                      textureInfo->textureProperties);
            }
        }
    }
}

void AssetManager::scanForRenderObjects()
{
    const std::vector<std::filesystem::path> willModels = file::findWillFiles(relative(std::filesystem::current_path() / "assets"), ".willmodel");
    renderObjects.reserve(willModels.size());
    for (std::filesystem::path willModel : willModels) {
        std::optional<RenderObjectInfo> renderObjectInfo = Serializer::loadWillModel(willModel);
        if (renderObjectInfo.has_value()) {
            if (!renderObjects.contains(renderObjectInfo->id)) {
                // todo: split here for other types
                renderObjects[renderObjectInfo->id] = std::make_unique<RenderObjectGltf>(resourceManager, renderObjectInfo.value());
                bRenderObjectsCacheDirty = true;
            }
        }
    }
}

Texture* AssetManager::getTexture(const uint32_t textureId) const
{
    if (textures.contains(textureId)) {
        return textures.at(textureId).get();
    }

    return nullptr;
}

std::vector<Texture*> AssetManager::getAllTextures()
{
    std::vector<Texture*> result;
    result.reserve(textures.size());

    std::ranges::transform(textures, std::back_inserter(result), [](const auto& pair) { return pair.second.get(); });

    return result;
}

Texture* AssetManager::getAnyTexture() const
{
    return textures.begin()->second.get();
}

RenderObject* AssetManager::getRenderObject(const uint32_t renderObjectId) const
{
    if (renderObjects.contains(renderObjectId)) {
        return renderObjects.at(renderObjectId).get();
    }

    return nullptr;
}

const std::vector<RenderObject*>& AssetManager::getAllRenderObjects()
{
    if (bRenderObjectsCacheDirty) {
        cachedRenderObjects.clear();
        cachedRenderObjects.reserve(renderObjects.size());
        std::ranges::transform(renderObjects, std::back_inserter(cachedRenderObjects),
                               [](const auto& pair) { return pair.second.get(); });
        bRenderObjectsCacheDirty = false;
    }
    return cachedRenderObjects;
}

RenderObject* AssetManager::getAnyRenderObject() const
{
    if (!hasAnyRenderObjects()) {
        return nullptr;
    }

    return renderObjects.begin()->second.get();
}
}
