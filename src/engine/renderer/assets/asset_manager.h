//
// Created by William on 2025-03-15.
//

#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H
#include <unordered_map>

#include "material/material.h"
#include "render_object/render_object.h"
#include "texture/texture.h"


namespace will_engine::renderer
{
class AssetManager
{
public:
    explicit AssetManager(renderer::ResourceManager& resourceManager);

    ~AssetManager();

    void scanForAll();

    void scanForTextures();

    void scanForRenderObjects();

public: // Textures
    Texture* getTexture(uint32_t textureId) const;

    std::vector<Texture*> getAllTextures();

    bool hasAnyTexture() const { return textures.size() > 0; }

    Texture* getAnyTexture() const;

private: // Textures
    std::unordered_map<uint32_t, std::unique_ptr<Texture> > textures;

public: // Render Objects

    RenderObject* getRenderObject(uint32_t renderObjectId) const;

    std::vector<RenderObject*> getAllRenderObjects();

    bool hasAnyRenderObjects() const { return renderObjects.size() > 0; }

    RenderObject* getAnyRenderObject() const;

private: // Render Objects
    std::unordered_map<uint32_t, std::unique_ptr<RenderObject> > renderObjects;

    std::unordered_map<uint32_t, std::unique_ptr<Material> > materials;

private:
    ResourceManager& resourceManager;
};
}


#endif //ASSET_MANAGER_H
