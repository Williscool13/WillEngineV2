//
// Created by William on 2024-08-24.
//

#include "input.h"

Input::Input()
{
    keyStateData[SDLK_ESCAPE] = {};
    keyStateData[SDLK_w] = {};
    keyStateData[SDLK_a] = {};
    keyStateData[SDLK_s] = {};
    keyStateData[SDLK_d] = {};
    keyStateData[SDLK_SPACE] = {};
    keyStateData[SDLK_c] = {};
    keyStateData[SDLK_LSHIFT] = {};
    keyStateData[SDLK_RETURN] = {};

    mouseStateData[SDL_BUTTON_LEFT - 1] = {};
    mouseStateData[SDL_BUTTON_RIGHT - 1] = {};
    mouseStateData[SDL_BUTTON_MIDDLE - 1] = {};
    mouseStateData[SDL_BUTTON(4) - 1] = {};
    mouseStateData[SDL_BUTTON(5) - 1] = {};
}

void Input::processEvent(const SDL_Event& event)
{
    switch (event.type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP: {
            const auto it = keyStateData.find(event.key.keysym.sym);
            if (it != keyStateData.end()) {
                UpdateInputState(it->second, event.type == SDL_KEYDOWN);
            }
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            const auto it = mouseStateData.find(event.button.button - 1);
            if (it != mouseStateData.end()) {
                UpdateInputState(it->second, event.type == SDL_MOUSEBUTTONDOWN);
            }
            break;
        }
        case SDL_MOUSEMOTION:
            mouseDeltaX += static_cast<float>(event.motion.xrel);
            mouseDeltaY += static_cast<float>(event.motion.yrel);
            mouseX = static_cast<float>(event.motion.x);
            mouseY = static_cast<float>(event.motion.y);
            break;
        default:
            break;
    }
}

void Input::updateFocus(Uint32 sdlWindowFlags)
{
    inFocus = (sdlWindowFlags & SDL_WINDOW_INPUT_FOCUS) != 0;
}

void Input::frameReset()
{
    for (std::pair<const int, InputStateData>& key: keyStateData) {
        key.second.pressed = false;
        key.second.released = false;
    }

    for (std::pair<const uint8_t, InputStateData>& mouse: mouseStateData) {
        mouse.second.pressed = false;
        mouse.second.released = false;
    }

    mouseDeltaX = 0;
    mouseDeltaY = 0;
}

void Input::UpdateInputState(InputStateData& inputButton, bool isPressed)
{
    inputButton.state = isPressed ? InputState::DOWN : InputState::UP;
    inputButton.pressed = isPressed;
    inputButton.released = !isPressed;
}


bool Input::isKeyPressed(SDL_Keycode key) const
{
    auto it = keyStateData.find(key);
    return it != keyStateData.end() && it->second.pressed;
}

bool Input::isKeyReleased(SDL_Keycode key) const
{
    auto it = keyStateData.find(key);
    return it != keyStateData.end() && it->second.released;
}

bool Input::isKeyDown(SDL_Keycode key) const
{
    auto it = keyStateData.find(key);
    return it != keyStateData.end() && it->second.state == InputState::DOWN;
}

bool Input::isMousePressed(uint8_t mouseButton) const
{
    auto it = mouseStateData.find(mouseButton);
    return it != mouseStateData.end() && it->second.pressed;
}

bool Input::isMouseReleased(uint8_t mouseButton) const
{
    auto it = mouseStateData.find(mouseButton);
    return it != mouseStateData.end() && it->second.released;
}

bool Input::isMouseDown(uint8_t mouseButton) const
{
    auto it = mouseStateData.find(mouseButton);
    return it != mouseStateData.end() && it->second.state == InputState::DOWN;
}

Input::InputStateData Input::getKeyData(SDL_Keycode key) const
{
    auto it = keyStateData.find(key);
    if (it != keyStateData.end()) {
        return it->second;
    }
    return {};
}

Input::InputStateData Input::getMouseData(uint8_t mouseButton) const
{
    auto it = mouseStateData.find(mouseButton);
    if (it != mouseStateData.end()) {
        return it->second;
    }
    return {};
}
