//
// Created by William on 2024-12-15.
//

#ifndef PLAYER_H
#define PLAYER_H
#include "src/core/game_object.h"
#include "src/physics/physics_filters.h"


class Camera;

class PlayerCharacter : public GameObject
{
public:
    PlayerCharacter();

    ~PlayerCharacter() override;

    void update(float deltaTime) override;

    [[nodiscard]] const Camera* getCamera() const { return camera; }

private:
    void addForceToObject() const;

protected:
    Camera* camera{nullptr};
};


class PlayerCollisionFilter final : public JPH::ObjectLayerFilter {
public:
    [[nodiscard]] bool ShouldCollide(const JPH::ObjectLayer inLayer) const override {
        return inLayer != Layers::PLAYER;
    }
};

#endif //PLAYER_H
