//
// Created by William on 2025-03-16.
//

#include "terrain_component.h"

#include "imgui.h"
#include "engine/core/engine.h"

namespace will_engine::game
{
TerrainComponent::TerrainComponent(const std::string& name) : Component(name)
{}

TerrainComponent::~TerrainComponent() = default;

void TerrainComponent::beginPlay()
{
    Component::beginPlay();
}

void TerrainComponent::update(const float deltaTime)
{
    Component::update(deltaTime);
}

void TerrainComponent::beginDestroy()
{
    Component::beginDestroy();


    if (Engine* engine = Engine::get()) {
        engine->removeFromActiveTerrain(this);
    }
}

void TerrainComponent::serialize(ordered_json& j)
{
    Component::serialize(j);

    j["terrain"]["bIsGenerated"] = bIsGenerated;
    j["terrain"]["terrainGenerationProperties"] = terrainGenerationProperties;
    j["terrain"]["terrainSeed"] = seed;
    j["terrain"]["terrainConfig"] = terrainConfig;
    if (terrainChunk) {
        j["terrain"]["terrainProperties"] = terrainChunk->getTerrainProperties();
        j["terrain"]["terrainTextures"] = terrainChunk->getTerrainTextureIds();
    }
}

void TerrainComponent::deserialize(ordered_json& j)
{
    Component::deserialize(j);

    if (j.contains("terrain")) {
        const ordered_json terrain = j["terrain"];

        if (terrain.contains("bIsGenerated")) {
            bIsGenerated = terrain["bIsGenerated"].get<bool>();
        }

        if (terrain.contains("terrainGenerationProperties")) {
            terrainGenerationProperties = terrain["terrainGenerationProperties"];
        }

        if (terrain.contains("terrainSeed")) {
            seed = terrain["terrainSeed"];
        }

        if (terrain.contains("terrainConfig")) {
            terrainConfig = terrain["terrainConfig"];
        }

        terrain::TerrainProperties tempProperties{};
        if (terrain.contains("terrainProperties")) {
            tempProperties = terrain["terrainProperties"];
        }

        std::array<uint32_t, terrain::MAX_TERRAIN_TEXTURE_COUNT> tempTextures = terrain::TerrainChunk::getDefaultTextureIds();
        if (terrain.contains("terrainTextures")) {
            auto& textureArray = terrain["terrainTextures"];
            if (textureArray.is_array()) {
                for (size_t i = 0; i < std::min(textureArray.size(), static_cast<size_t>(terrain::MAX_TERRAIN_TEXTURE_COUNT)); i++) {
                    tempTextures[i] = textureArray[i].get<uint32_t>();
                }
            }
        }

        if (bIsGenerated) {
            generateTerrain(tempProperties, tempTextures);
        }
    }
}

void TerrainComponent::generateTerrain()
{
    std::vector<float> heightMapData =
            HeightmapUtil::generateFromNoise(NOISE_MAP_DIMENSIONS, NOISE_MAP_DIMENSIONS, seed, terrainGenerationProperties);
    terrainChunk = std::make_unique<terrain::TerrainChunk>(*Engine::get()->getResourceManager(), heightMapData, NOISE_MAP_DIMENSIONS,
                                                           NOISE_MAP_DIMENSIONS, terrainConfig);
    if (Engine* engine = Engine::get()) {
        engine->addToActiveTerrain(this);
    }
    bIsGenerated = true;
}

void TerrainComponent::generateTerrain(terrain::TerrainProperties terrainProperties,
                                                                std::array<uint32_t, terrain::MAX_TERRAIN_TEXTURE_COUNT> textureIds)
{
    std::vector<float> heightMapData =
            HeightmapUtil::generateFromNoise(NOISE_MAP_DIMENSIONS, NOISE_MAP_DIMENSIONS, seed, terrainGenerationProperties);
    terrainChunk = std::make_unique<terrain::TerrainChunk>(*Engine::get()->getResourceManager(), heightMapData, NOISE_MAP_DIMENSIONS,
                                                           NOISE_MAP_DIMENSIONS, terrainConfig);
    if (Engine* engine = Engine::get()) {
        engine->addToActiveTerrain(this);
    }
    terrainChunk->setTerrainBufferData(terrainProperties, textureIds);
    bIsGenerated = true;
}

void TerrainComponent::destroyTerrain()
{
    terrainChunk.reset();
    bIsGenerated = false;
}

std::vector<float> TerrainComponent::getHeightMapData() const
{
    return HeightmapUtil::generateFromNoise(NOISE_MAP_DIMENSIONS, NOISE_MAP_DIMENSIONS, seed, terrainGenerationProperties);
}

void TerrainComponent::updateRenderImgui()
{
    Component::updateRenderImgui();

    ImGui::Text("This component is not meant to be added directly to game objects. This will have no effect.\n");
}
}
