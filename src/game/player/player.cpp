//
// Created by William on 2024-12-15.
//

#include "player.h"

#include <fmt/format.h>

#include "src/core/input.h"
#include "src/core/camera/camera.h"
#include "src/game/camera/free_camera.h"

Player::Player()
{
    camera = new FreeCamera(75.0f, 1920.0f / 1080.0f, 1000, 0.01);
}

Player::~Player()
{
    delete camera;
}

void Player::update(float deltaTime)
{
    if (camera) { camera->update(); }

    const Input& input = Input::Get();

    if (input.isKeyPressed(SDLK_e)) {
        fmt::print("F");
    }
}
