//
// Created by William on 2024-12-15.
//

#ifndef PLAYER_H
#define PLAYER_H
#include "src/core/game_object.h"
#include "src/physics/physics_filters.h"


class FreeCamera;
class OrbitCamera;
class Camera;


class PlayerCharacter : public GameObject
{
private:
    enum CameraType
    {
        Free,
        Orbit
    };

public:
    PlayerCharacter() = delete;

    explicit PlayerCharacter(const std::string& gameObjectName);

    ~PlayerCharacter() override;

    void update(float deltaTime) override;

    [[nodiscard]] const Camera* getCamera() const { return currentCamera; }

public: // Debug
    bool isUsingDebugCamera() const;
    const FreeCamera* getFreeCamera() const { return freeCamera; }
    const OrbitCamera* getOrbitCamera() const { return orbitCamera; }

private:
    void addForceToObject() const;

protected:
    Camera* currentCamera{nullptr};

    OrbitCamera* orbitCamera{nullptr};
    FreeCamera* freeCamera{nullptr};

    FreeCamera* debugCamera{nullptr};
};


class PlayerCollisionFilter final : public JPH::ObjectLayerFilter
{
public:
    [[nodiscard]] bool ShouldCollide(const JPH::ObjectLayer inLayer) const override
    {
        return inLayer != Layers::PLAYER;
    }
};

#endif //PLAYER_H
