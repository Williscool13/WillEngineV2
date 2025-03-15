//
// Created by William on 2025-03-15.
//

#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H
#include <unordered_map>

#include "material/material.h"
#include "render_object/render_object.h"
#include "texture/texture.h"


namespace will_engine
{
class AssetManager
{
public:
    explicit AssetManager(ResourceManager& resourceManager);

    ~AssetManager();

public:
    void scanForRenderObjects();

public:
    const std::unordered_map<uint32_t, std::unique_ptr<RenderObject> >& getRenderObjects() { return renderObjects; }

private:
    std::unordered_map<uint32_t, std::unique_ptr<RenderObject> > renderObjects;
    std::unordered_map<uint32_t, std::unique_ptr<Texture> > textures;
    std::unordered_map<uint32_t, std::unique_ptr<Material> > materials;

private:
    ResourceManager& resourceManager;
};
}


#endif //ASSET_MANAGER_H
