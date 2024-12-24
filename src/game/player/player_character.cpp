//
// Created by William on 2024-12-15.
//

#include "player_character.h"

#include <fmt/format.h>

#include "src/core/game_object.h"
#include "src/core/input.h"
#include "src/core/camera/camera.h"
#include "src/game/camera/free_camera.h"
#include "src/game/camera/orbit_camera.h"
#include "src/physics/physics_utils.h"

PlayerCharacter::PlayerCharacter()
{
    //camera = new FreeCamera(75.0f, 1920.0f / 1080.0f, 1000, 0.01);
    camera = new OrbitCamera(75.0f, 1920.0f / 1080.0f, 1000, 0.01);
    if (const auto orbit = dynamic_cast<OrbitCamera*>(camera)){
        orbit->setOrbitTarget(this);
    }
}

PlayerCharacter::~PlayerCharacter()
{
    delete camera;
}

void PlayerCharacter::update(float deltaTime)
{
    if (camera) { camera->update(deltaTime); }

    const Input& input = Input::Get();

    if (input.isKeyPressed(SDLK_e)) {
        addForceToObject();
    }

    if (input.isKeyDown(SDLK_w)) {

    }
}

void PlayerCharacter::addForceToObject() const
{
    const glm::vec3 direction = camera->transform.getForward();
    const RaycastHit result = physics_utils::raycast(camera->getPosition(), direction, 100.0f);

    if (result.hasHit) {
        const GameObject* object = physics_utils::getGameObjectFromBody(result.hitBodyID);
        if (object) {
            physics_utils::addImpulseAtPosition(result.hitBodyID, normalize(direction) * 10000.0f, result.hitPosition);
            fmt::print("Object found: {}\n", object->getName());
        } else {
            // e.g. pre-defined world floor
            fmt::print("Raycast hit but no object found\n");
        }

    } else {
        fmt::print("Failed to find an object with the raycast\n");
    }
}

