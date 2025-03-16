//
// Created by William on 2025-03-16.
//

#include "terrain_component.h"

#include "src/core/engine.h"

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

    j["terrain"]["terrainProperties"] = terrainProperties;
    j["terrain"]["terrainSeed"] = seed;
}

void will_engine::components::TerrainComponent::deserialize(ordered_json& j)
{
    Component::deserialize(j);

    if (j.contains("terrain")) {
        const ordered_json terrain = j["terrain"];

        bool shouldLoad = false;
        if (terrain.contains("terrainProperties")) {
            terrainProperties = terrain["terrainProperties"];
            shouldLoad = true;
        }

        if (terrain.contains("terrainSeed")) {
            seed = terrain["terrainSeed"];
            shouldLoad = true;
        }

        if (shouldLoad) {
            generateTerrain();
        }
    }
}

void will_engine::components::TerrainComponent::generateTerrain()
{
    std::vector<float> heightMapData = HeightmapUtil::generateFromNoise(NOISE_MAP_DIMENSIONS, NOISE_MAP_DIMENSIONS, seed, terrainProperties);
    terrainChunk = std::make_unique<terrain::TerrainChunk>(*Engine::get()->getResourceManager(), heightMapData, NOISE_MAP_DIMENSIONS, NOISE_MAP_DIMENSIONS);
    if (Engine* engine = Engine::get()) {
        engine->addToActiveTerrain(this);
    }
}

void will_engine::components::TerrainComponent::destroyTerrain()
{
    terrainChunk.reset();
}

std::vector<float> will_engine::components::TerrainComponent::getHeightMapData() const
{
    return HeightmapUtil::generateFromNoise(NOISE_MAP_DIMENSIONS, NOISE_MAP_DIMENSIONS, seed, terrainProperties);
}
