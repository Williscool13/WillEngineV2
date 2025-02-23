//
// Created by William on 2025-02-23.
//

#ifndef TERRAIN_MANAGER_H
#define TERRAIN_MANAGER_H
#include "terrain_types.h"
#include "src/renderer/resource_manager.h"


namespace will_engine::terrain {

class TerrainManager {
    static TerrainManager* Get() { return instance; }
    static void Set(TerrainManager* manager) { instance = manager; }
    static TerrainManager* instance;

    explicit TerrainManager(ResourceManager& resourceManager)
        : resourceManager(resourceManager) {}


private:
    ResourceManager& resourceManager;
};

} // terrain
// will_engine

#endif //TERRAIN_MANAGER_H
