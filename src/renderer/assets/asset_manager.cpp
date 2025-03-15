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

void will_engine::AssetManager::scanForRenderObjects()
{
    fmt::print("Scanning for .willmodel files\n");

    const std::vector<std::filesystem::path> willModels = file::findWillmodels(relative(std::filesystem::current_path() / "assets"));
    renderObjects.reserve(willModels.size());
    for (std::filesystem::path willModel : willModels) {
        std::optional<RenderObjectInfo> renderObjectInfo = Serializer::loadWillModel(willModel);
        if (renderObjectInfo.has_value()) {
            if (!renderObjects.contains(renderObjectInfo->id)) {
                renderObjects[renderObjectInfo->id] = std::make_unique<RenderObject>(resourceManager, std::filesystem::path(renderObjectInfo->gltfPath), renderObjectInfo->name, renderObjectInfo->id);
            }
        }
    }
}
