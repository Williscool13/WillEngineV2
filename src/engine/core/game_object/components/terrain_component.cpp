//
// Created by William on 2025-03-16.
//

#include "terrain_component.h"

#include "imgui.h"
#include "engine/core/engine.h"

will_engine::components::TerrainComponent::TerrainComponent(const std::string& name) : Component(name)
{}

will_engine::components::TerrainComponent::~TerrainComponent() = default;

void will_engine::components::TerrainComponent::beginPlay()
{
    Component::beginPlay();
}

void will_engine::components::TerrainComponent::update(const float deltaTime)
{
    Component::update(deltaTime);
}

void will_engine::components::TerrainComponent::beginDestroy()
{
    Component::beginDestroy();


    if (Engine* engine = Engine::get()) {
        engine->removeFromActiveTerrain(this);
    }
}

void will_engine::components::TerrainComponent::serialize(ordered_json& j)
{
    Component::serialize(j);

    j["terrain"]["terrainGenerationProperties"] = terrainGenerationProperties;
    j["terrain"]["terrainSeed"] = seed;
    j["terrain"]["terrainConfig"] = terrainConfig;
    if (terrainChunk) {
        j["terrain"]["terrainProperties"] = terrainChunk->getTerrainProperties();
        j["terrain"]["terrainTextures"] = terrainChunk->getTerrainTextureIds();
    }
}

void will_engine::components::TerrainComponent::deserialize(ordered_json& j)
{
    Component::deserialize(j);

    if (j.contains("terrain")) {
        const ordered_json terrain = j["terrain"];

        bool shouldLoad = false;
        if (terrain.contains("terrainGenerationProperties")) {
            terrainGenerationProperties = terrain["terrainGenerationProperties"];
            shouldLoad = true;
        }

        if (terrain.contains("terrainSeed")) {
            seed = terrain["terrainSeed"];
            shouldLoad = true;
        }

        if (terrain.contains("terrainConfig")) {
            terrainConfig = terrain["terrainConfig"];
            shouldLoad = true;
        }

        terrain::TerrainProperties tempProperties{};
        if (terrain.contains("terrainProperties")) {
            tempProperties = terrain["terrainProperties"];
            shouldLoad = true;
        }

        std::array<uint32_t, terrain::MAX_TERRAIN_TEXTURE_COUNT> tempTextures = terrain::TerrainChunk::getDefaultTextureIds();
        if (terrain.contains("terrainTextures")) {
            auto& textureArray = terrain["terrainTextures"];
            if (textureArray.is_array()) {
                for (size_t i = 0; i < std::min(textureArray.size(), static_cast<size_t>(terrain::MAX_TERRAIN_TEXTURE_COUNT)); i++) {
                    tempTextures[i] = textureArray[i].get<uint32_t>();
                }
            }
            shouldLoad = true;
        }

        if (shouldLoad) {
            generateTerrain(tempProperties, tempTextures);
        }
    }
}

void will_engine::components::TerrainComponent::generateTerrain()
{
    std::vector<float> heightMapData = HeightmapUtil::generateFromNoise(NOISE_MAP_DIMENSIONS, NOISE_MAP_DIMENSIONS, seed, terrainGenerationProperties);
    terrainChunk = std::make_unique<terrain::TerrainChunk>(*Engine::get()->getResourceManager(), heightMapData, NOISE_MAP_DIMENSIONS, NOISE_MAP_DIMENSIONS, terrainConfig);
    if (Engine* engine = Engine::get()) {
        engine->addToActiveTerrain(this);
    }
}

void will_engine::components::TerrainComponent::generateTerrain(terrain::TerrainProperties terrainProperties, std::array<uint32_t, terrain::MAX_TERRAIN_TEXTURE_COUNT> textureIds)
{
    std::vector<float> heightMapData = HeightmapUtil::generateFromNoise(NOISE_MAP_DIMENSIONS, NOISE_MAP_DIMENSIONS, seed, terrainGenerationProperties);
    terrainChunk = std::make_unique<terrain::TerrainChunk>(*Engine::get()->getResourceManager(), heightMapData, NOISE_MAP_DIMENSIONS, NOISE_MAP_DIMENSIONS, terrainConfig);
    if (Engine* engine = Engine::get()) {
        engine->addToActiveTerrain(this);
    }
    terrainChunk->setTerrainBufferData(terrainProperties, textureIds);
}

void will_engine::components::TerrainComponent::destroyTerrain()
{
    terrainChunk.reset();
}

std::vector<float> will_engine::components::TerrainComponent::getHeightMapData() const
{
    return HeightmapUtil::generateFromNoise(NOISE_MAP_DIMENSIONS, NOISE_MAP_DIMENSIONS, seed, terrainGenerationProperties);
}

void will_engine::components::TerrainComponent::updateRenderImgui()
{
    Component::updateRenderImgui();

    ImGui::Text("This component is not meant to be added directly to game objects. This will have no effect.");
}
