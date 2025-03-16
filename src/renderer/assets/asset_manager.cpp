//
// Created by William on 2025-03-15.
//

#include "asset_manager.h"

#include "src/util/file.h"

will_engine::AssetManager::AssetManager(ResourceManager& resourceManager) : resourceManager(resourceManager)
{}

will_engine::AssetManager::~AssetManager()
{
    for (auto& texture : textures) {
        texture.second.release();
    }
}

void will_engine::AssetManager::scanForAll()
{
    scanForRenderObjects();
    scanForTextures();
}

void will_engine::AssetManager::scanForTextures()
{
    fmt::print("Scanning for .willtexture files\n");

    const std::vector<std::filesystem::path> willTextures = file::findWillFiles(relative(std::filesystem::current_path() / "assets"), ".willtexture");
    textures.reserve(willTextures.size());
    for (std::filesystem::path willTexture : willTextures) {
        std::optional<TextureInfo> textureInfo = Serializer::loadWillTexture(willTexture);
        if (textureInfo.has_value()) {
            if (!textures.contains(textureInfo->id)) {
                textures[textureInfo->id] = std::make_unique<Texture>(resourceManager, textureInfo->id, textureInfo->willtexturePath, std::filesystem::path(textureInfo->texturePath),
                                                                      textureInfo->textureProperties);
            }
        }
    }
}

void will_engine::AssetManager::scanForRenderObjects()
{
    fmt::print("Scanning for .willmodel files\n");

    const std::vector<std::filesystem::path> willModels = file::findWillFiles(relative(std::filesystem::current_path() / "assets"), ".willmodel");
    renderObjects.reserve(willModels.size());
    for (std::filesystem::path willModel : willModels) {
        std::optional<RenderObjectInfo> renderObjectInfo = Serializer::loadWillModel(willModel);
        if (renderObjectInfo.has_value()) {
            if (!renderObjects.contains(renderObjectInfo->id)) {
                renderObjects[renderObjectInfo->id] = std::make_unique<RenderObject>(resourceManager, renderObjectInfo->willmodelPath, std::filesystem::path(renderObjectInfo->gltfPath),
                                                                                     renderObjectInfo->name, renderObjectInfo->id);
            }
        }
    }
}

will_engine::RenderObject* will_engine::AssetManager::getRenderObject(const uint32_t renderObjectId) const
{
    if (renderObjects.contains(renderObjectId)) {
        return renderObjects.at(renderObjectId).get();
    }

    return nullptr;
}

std::vector<will_engine::RenderObject*> will_engine::AssetManager::getAllRenderObjects()
{
    std::vector<RenderObject*> result;
    result.reserve(renderObjects.size());

    std::ranges::transform(renderObjects, std::back_inserter(result), [](const auto& pair) { return pair.second.get(); });

    return result;
}

will_engine::RenderObject* will_engine::AssetManager::getAnyRenderObject() const
{
    return renderObjects.begin()->second.get();
}
