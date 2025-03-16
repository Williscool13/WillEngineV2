//
// Created by William on 2025-03-16.
//

#ifndef TERRAIN_COMPONENT_H
#define TERRAIN_COMPONENT_H
#include "component.h"
#include "src/core/game_object/terrain.h"
#include "src/util/heightmap_utils.h"

namespace will_engine::components
{
class TerrainComponent : public Component, public ITerrain
{
public:
    explicit TerrainComponent(const std::string& name);

    ~TerrainComponent() override;

public:
    void beginPlay() override;

    void update(float deltaTime) override;

    void beginDestroy() override;

    void serialize(ordered_json& j) override;

    void deserialize(ordered_json& j) override;

    terrain::TerrainChunk* getTerrainChunk() override { return terrainChunk.get(); }

    void generateTerrain() override;

    void destroyTerrain() override;

    void generateTerrain(const uint32_t seed)
    {
        this->seed = seed;
        generateTerrain();
    }

    void generateTerrain(const NoiseSettings& terrainProperties)
    {
        this->terrainProperties = terrainProperties;
        generateTerrain();
    }

    void generateTerrain(const NoiseSettings& terrainProperties, const uint32_t seed)
    {
        this->terrainProperties = terrainProperties;
        this->seed = seed;
        generateTerrain();
    }

    NoiseSettings getTerrainProperties() const { return terrainProperties; }
    uint32_t getSeed() const { return seed; }

    std::vector<float> getHeightMapData() const;

public:
    static constexpr auto TYPE = "TerrainComponent";
    static constexpr bool CAN_BE_CREATED_MANUALLY = false;

    static std::string_view getStaticType()
    {
        return TYPE;
    }

    std::string_view getComponentType() override { return TYPE; }

private:
    std::unique_ptr<terrain::TerrainChunk> terrainChunk;

    const float DEFAULT_TERRAIN_SCALE{100.0f};
    const float DEFAULT_TERRAIN_HEIGHT_SCALE{50.0f};

    NoiseSettings terrainProperties{
        .scale = DEFAULT_TERRAIN_SCALE,
        .persistence = 0.5f,
        .lacunarity = 2.0f,
        .octaves = 4,
        .offset = {0.0f, 0.0f},
        .heightScale = DEFAULT_TERRAIN_HEIGHT_SCALE
    };
    uint32_t seed{13};

public:
    void updateRenderImgui() override;
};
}


#endif //TERRAIN_COMPONENT_H
