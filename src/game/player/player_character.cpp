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
#include "src/physics/physics_filters.h"
#include "src/physics/physics_utils.h"

PlayerCharacter::PlayerCharacter(const std::string& gameObjectName) : GameObject(gameObjectName)
{
    freeCamera = new FreeCamera(75.0f, 1920.0f / 1080.0f, 10000, 0.1);
    freeCamera->transform.translate({0.0f, 1.5f, +1.5f});
    freeCamera->updateViewMatrix();

    orbitCamera = new OrbitCamera(75.0f, 1920.0f / 1080.0f, 10000, 0.1);
    orbitCamera->setOrbitTarget(this);

    currentCamera = freeCamera;
}

PlayerCharacter::~PlayerCharacter()
{
    delete freeCamera;
    delete orbitCamera;
}

void PlayerCharacter::update(const float deltaTime)
{
    const Input& input = Input::Get();
    if (!input.isInFocus()) { return; }

    if (input.isKeyPressed(SDLK_e)) {
        addForceToObject();
    }

    if (input.isKeyPressed(SDLK_1)) {
        if (currentCamera != freeCamera){
            currentCamera = freeCamera;
            if (freeCamera && orbitCamera) {
                freeCamera->transform.setPosition(orbitCamera->transform.getPosition());
                freeCamera->transform.setRotation(orbitCamera->transform.getRotation());
            }
        }

    }
    if (input.isKeyPressed(SDLK_2)) {
        if (currentCamera != orbitCamera) {
            currentCamera = orbitCamera;
        }
    }

    if (currentCamera && currentCamera == orbitCamera) {
        const glm::quat cameraRotation = currentCamera->transform.getRotation();
        const glm::vec3 forward = cameraRotation * glm::vec3(0.0f, 0.0f, -1.0f);
        const glm::vec3 right = cameraRotation * glm::vec3(-1.0f, 0.0f, 0.0f);
        constexpr float forceStrength = 10000.0f;

        if (input.isKeyDown(SDLK_w)) { physics_utils::addForce(gameObjectId, forward * forceStrength); }
        if (input.isKeyDown(SDLK_s)) { physics_utils::addForce(gameObjectId, -forward * forceStrength); }
        if (input.isKeyDown(SDLK_a)) { physics_utils::addForce(gameObjectId, right * forceStrength); }
        if (input.isKeyDown(SDLK_d)) { physics_utils::addForce(gameObjectId, -right * forceStrength); }
    }

    if (currentCamera) { currentCamera->update(deltaTime); }
}

void PlayerCharacter::addForceToObject() const
{
    if (!currentCamera) { return; }
    const glm::vec3 direction = currentCamera->transform.getForward();
    const PlayerCollisionFilter dontHitPlayerFilter{};
    const RaycastHit result = physics_utils::raycast(currentCamera->getPosition(), direction, 100.0f, {}, dontHitPlayerFilter, {});

    if (result.hasHit) {
        if (const GameObject* object = physics_utils::getGameObjectFromBody(result.hitBodyID)) {
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
