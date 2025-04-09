//
// Created by William on 2024-08-24.
//

#include "input.h"

#include <fmt/format.h>

#include "imgui.h"
#include "src/engine_constants.h"

namespace will_engine
{
Input::Input()
{
    keyStateData[SDLK_ESCAPE] = {};
    keyStateData[SDLK_SPACE] = {};
    keyStateData[SDLK_LSHIFT] = {};
    keyStateData[SDLK_LCTRL] = {};
    keyStateData[SDLK_RETURN] = {};
    keyStateData[SDLK_Q] = {};
    keyStateData[SDLK_W] = {};
    keyStateData[SDLK_E] = {};
    keyStateData[SDLK_R] = {};
    keyStateData[SDLK_T] = {};
    keyStateData[SDLK_Y] = {};
    keyStateData[SDLK_U] = {};
    keyStateData[SDLK_I] = {};
    keyStateData[SDLK_O] = {};
    keyStateData[SDLK_P] = {};
    keyStateData[SDLK_A] = {};
    keyStateData[SDLK_S] = {};
    keyStateData[SDLK_D] = {};
    keyStateData[SDLK_F] = {};
    keyStateData[SDLK_G] = {};
    keyStateData[SDLK_H] = {};
    keyStateData[SDLK_J] = {};
    keyStateData[SDLK_K] = {};
    keyStateData[SDLK_L] = {};
    keyStateData[SDLK_Z] = {};
    keyStateData[SDLK_X] = {};
    keyStateData[SDLK_C] = {};
    keyStateData[SDLK_V] = {};
    keyStateData[SDLK_B] = {};
    keyStateData[SDLK_N] = {};
    keyStateData[SDLK_M] = {};
    keyStateData[SDLK_COMMA] = {};
    keyStateData[SDLK_PERIOD] = {};
    keyStateData[SDLK_1] = {};
    keyStateData[SDLK_2] = {};
    keyStateData[SDLK_3] = {};
    keyStateData[SDLK_4] = {};
    keyStateData[SDLK_5] = {};
    keyStateData[SDLK_6] = {};
    keyStateData[SDLK_7] = {};
    keyStateData[SDLK_8] = {};
    keyStateData[SDLK_9] = {};
    keyStateData[SDLK_0] = {};

    mouseStateData[SDL_BUTTON_LEFT - 1] = {};
    mouseStateData[SDL_BUTTON_RIGHT - 1] = {};
    mouseStateData[SDL_BUTTON_MIDDLE - 1] = {};
    mouseStateData[SDL_BUTTON_X1] = {};
    mouseStateData[SDL_BUTTON_X2] = {};
}

void Input::init(SDL_Window* window)
{
    this->window = window;
}

void Input::processEvent(const SDL_Event& event)
{
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        {
            const auto it = keyStateData.find(event.key.key);
            if (it != keyStateData.end()) {
                UpdateInputState(it->second, event.key.down);
            }
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        {
            const auto it = mouseStateData.find(event.button.button - 1);
            if (it != mouseStateData.end()) {
                UpdateInputState(it->second, event.button.down);
            }
            break;
        }
        case SDL_EVENT_MOUSE_MOTION:
        {
            mouseXDelta += static_cast<float>(event.motion.xrel);
            mouseYDelta += static_cast<float>(event.motion.yrel);
            mouseX = static_cast<float>(event.motion.x);
            mouseY = static_cast<float>(event.motion.y);
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL:
        {
            mouseWheelDelta += event.wheel.mouse_y;
            break;
        }
        default:
            break;
    }
}

void Input::updateFocus(const Uint32 sdlWindowFlags)
{
    windowInputFocus = (sdlWindowFlags & SDL_WINDOW_INPUT_FOCUS) != 0;
    const bool isTyping = engine_constants::useImgui ? ImGui::GetIO().WantTextInput : false;
    const bool isPopupActive = engine_constants::useImgui ? ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopup) : false;

    if (windowInputFocus && !isTyping && !isPopupActive && isKeyPressed(SDLK_F)) {
        inFocus = !inFocus;
        if (!window) {
            fmt::print("Input: Attempted to update focus but window is not defined, perhaps init was not called?\n");
            return;
        }
        SDL_SetWindowRelativeMouseMode(window, inFocus);
    }
}

void Input::frameReset()
{
    for (std::pair<const unsigned, InputStateData>& key : keyStateData) {
        key.second.pressed = false;
        key.second.released = false;
    }

    for (std::pair<const uint8_t, InputStateData>& mouse : mouseStateData) {
        mouse.second.pressed = false;
        mouse.second.released = false;
    }

    mouseXDelta = 0;
    mouseYDelta = 0;
}

void Input::UpdateInputState(InputStateData& inputButton, const bool isPressed)
{
    inputButton.state = isPressed ? InputState::DOWN : InputState::UP;
    inputButton.pressed = isPressed;
    inputButton.released = !isPressed;
}


bool Input::isKeyPressed(const SDL_Keycode key) const
{
    const auto it = keyStateData.find(key);
    return it != keyStateData.end() && it->second.pressed;
}

bool Input::isKeyReleased(const SDL_Keycode key) const
{
    const auto it = keyStateData.find(key);
    return it != keyStateData.end() && it->second.released;
}

bool Input::isKeyDown(const SDL_Keycode key) const
{
    const auto it = keyStateData.find(key);
    return it != keyStateData.end() && it->second.state == InputState::DOWN;
}

bool Input::isMousePressed(const uint8_t mouseButton) const
{
    const auto it = mouseStateData.find(mouseButton);
    return it != mouseStateData.end() && it->second.pressed;
}

bool Input::isMouseReleased(const uint8_t mouseButton) const
{
    const auto it = mouseStateData.find(mouseButton);
    return it != mouseStateData.end() && it->second.released;
}

bool Input::isMouseDown(const uint8_t mouseButton) const
{
    const auto it = mouseStateData.find(mouseButton);
    return it != mouseStateData.end() && it->second.state == InputState::DOWN;
}

Input::InputStateData Input::getKeyData(const SDL_Keycode key) const
{
    const auto it = keyStateData.find(key);
    if (it != keyStateData.end()) {
        return it->second;
    }
    return {};
}

Input::InputStateData Input::getMouseData(const uint8_t mouseButton) const
{
    const auto it = mouseStateData.find(mouseButton);
    if (it != mouseStateData.end()) {
        return it->second;
    }
    return {};
}
}
