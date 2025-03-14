//
// Created by William on 2025-03-08.
//

#ifndef MAP_H
#define MAP_H
#include <filesystem>
#include <json/json.hpp>

#include "src/core/game_object/hierarchical.h"
#include "src/core/game_object/terrain.h"
#include "src/core/game_object/transformable.h"
#include "src/renderer/terrain/terrain_chunk.h"
#include "src/util/heightmap_utils.h"

namespace will_engine
{
using ordered_json = nlohmann::ordered_json;

inline void to_json(ordered_json& j, const NoiseSettings& settings)
{
    j = {
        {"scale", settings.scale},
        {"persistence", settings.persistence},
        {"lacunarity", settings.lacunarity},
        {"octaves", settings.octaves},
        {
            "offset", {
                {"x", settings.offset.x},
                {"y", settings.offset.y}
            }
        },
        {"heightScale", settings.heightScale}
    };
}

inline void from_json(const ordered_json& j, NoiseSettings& settings)
{
    settings.scale = j["scale"].get<float>();
    settings.persistence = j["persistence"].get<float>();
    settings.lacunarity = j["lacunarity"].get<float>();
    settings.octaves = j["octaves"].get<int>();

    glm::vec2 offset{
        j["offset"]["x"].get<float>(),
        j["offset"]["y"].get<float>()
    };
    settings.offset = offset;

    settings.heightScale = j["heightScale"].get<float>();
}

/**
 * Maps do no have parents, they are the roots of their respctive hierarchies
 */
class Map final : public IHierarchical,
                  public ITransformable,
                  public ITerrain
{
public:
    explicit Map(const std::filesystem::path& mapSource, ResourceManager& resourceManager, bool initializeTerrain = false);

    ~Map() override;

    void destroy() override;

    bool saveMap(const std::filesystem::path& newSavePath = {});

    int32_t getMapId() const { return mapId; }

    std::filesystem::path getMapPath() const { return mapSource; }

    void addGameObject(IHierarchical* newChild);

public: // Terrain
    void generateTerrain();

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

    void destroyTerrain();

    AllocatedBuffer getVertexBuffer() override;

    AllocatedBuffer getIndexBuffer() override;

    size_t getIndicesCount() override;

    bool canDraw() override { return terrainChunk.get(); }

    NoiseSettings getTerrainProperties() const { return terrainProperties; }
    uint32_t getSeed() const { return seed; }

private: // Terrain
    std::unique_ptr<terrain::TerrainChunk> terrainChunk;
    const float DEFAULT_TERRAIN_SCALE = 100.0f;
    const float DEFAULT_TERRAIN_HEIGHT_SCALE = 50.0f;
    NoiseSettings terrainProperties{
        .scale = DEFAULT_TERRAIN_SCALE,
        .persistence = 0.5f,
        .lacunarity = 2.0f,
        .octaves = 4,
        .offset = {0.0f, 0.0f},
        .heightScale = DEFAULT_TERRAIN_HEIGHT_SCALE
    };
    uint32_t seed{13};

#pragma region Interfaces

public: // IHierarchical
    void beginPlay() override {};

    /**
     * Updates all gameobjects under this map
     * @param deltaTime
     */
    void update(float deltaTime) override;

    void beginDestroy() override {};

    bool addChild(IHierarchical* child) override;

    bool removeChild(IHierarchical* child) override;

    /**
     * Removes the parent while maintaining world position. Does not attempt to update the parent's state.
     * \n WARNING: Doing this without re-parenting will result in an orphaned gameobject, beware.
     * @return
     */
    bool removeParent() override;

    void reparent(IHierarchical* newParent) override;

    void dirty() override;

    void setParent(IHierarchical* newParent) override;

    IHierarchical* getParent() const override;

    const std::vector<IHierarchical*>& getChildren() const override;

    std::vector<IHierarchical*>& getChildren() override;

    void setName(std::string newName) override;

    std::string_view getName() const override { return mapName; }

private: // IHierarchical
    std::vector<IHierarchical*> children{};

    void recursiveUpdate(IHierarchical* object, float deltaTime);

    void recursiveDestroy(IHierarchical* object);

public: // ITransformable
    glm::mat4 getModelMatrix() override { return cachedModelMatrix; }

    const Transform& getLocalTransform() const override { return transform; }
    glm::vec3 getLocalPosition() const override { return getLocalTransform().getPosition(); }
    glm::quat getLocalRotation() const override { return getLocalTransform().getRotation(); }
    glm::vec3 getLocalScale() const override { return getLocalTransform().getScale(); }

    const Transform& getGlobalTransform() override { return transform; }

    /**
     * Interface of both IPhysicsBody and ITransformable
     * @return
     */
    glm::vec3 getPosition() override { return getGlobalTransform().getPosition(); }
    /**
     * Interface of both IPhysicsBody and ITransformable
     * @return
     */
    glm::quat getRotation() override { return getGlobalTransform().getRotation(); }
    glm::vec3 getScale() override { return getGlobalTransform().getScale(); }

    void setLocalPosition(glm::vec3 localPosition) override;

    void setLocalRotation(glm::quat localRotation) override;

    void setLocalScale(glm::vec3 localScale) override;

    void setLocalScale(float localScale) override;

    void setLocalTransform(const Transform& newLocalTransform) override;

    void setGlobalPosition(const glm::vec3 globalPosition) override { setLocalPosition(globalPosition); }

    void setGlobalRotation(const glm::quat globalRotation) override { setLocalRotation(globalRotation); }

    void setGlobalScale(const glm::vec3 newScale) override { setLocalScale(newScale); }

    void setGlobalScale(const float globalScale) override { setGlobalScale(glm::vec3(globalScale)); }

    void setGlobalTransform(const Transform& newGlobalTransform) override { setLocalTransform(newGlobalTransform); }

    void setGlobalTransformFromPhysics(const glm::vec3& position, const glm::quat& rotation) override {}

    void translate(glm::vec3 translation) override;

    void rotate(glm::quat rotation) override;

    void rotateAxis(float angle, const glm::vec3& axis) override;

private: // ITransformable
    Transform transform{};
    glm::mat4 cachedModelMatrix{1.0f};

#pragma endregion

private:
    std::filesystem::path mapSource{};
    std::string mapName{};
    uint32_t mapId;

    ResourceManager& resourceManager;

    bool isLoaded{false};
};
}


#endif //MAP_H
