//
// Created by William on 2024-08-24.
//

#include "input.h"

#include <ranges>
#include <fmt/format.h>

#include "imgui.h"
#include "engine/engine_constants.h"

namespace will_engine::input
{
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
            const auto it = keyStateData.find(static_cast<Key>(event.key.key));
            if (it != keyStateData.end()) {
                UpdateInputState(it->second, event.key.down);
            }
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        {
            const auto it = mouseStateData.find(static_cast<MouseButton>(SDL_BUTTON_MASK(event.button.button)));
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

    if (windowInputFocus && !isTyping && !isPopupActive && isKeyPressed(Key::NUMLOCKCLEAR)) {
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
    for (auto& val : keyStateData | std::views::values) {
        val.pressed = false;
        val.released = false;
    }

    for (auto& val : mouseStateData | std::views::values) {
        val.pressed = false;
        val.released = false;
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


bool Input::isKeyPressed(const Key key) const
{
    const auto it = keyStateData.find(key);
    return it != keyStateData.end() && it->second.pressed;
}

bool Input::isKeyReleased(const Key key) const
{
    const auto it = keyStateData.find(key);
    return it != keyStateData.end() && it->second.released;
}

bool Input::isKeyDown(const Key key) const
{
    const auto it = keyStateData.find(key);
    return it != keyStateData.end() && it->second.state == InputState::DOWN;
}

bool Input::isMousePressed(const MouseButton mouseButton) const
{
    const auto it = mouseStateData.find(mouseButton);
    return it != mouseStateData.end() && it->second.pressed;
}

bool Input::isMouseReleased(const MouseButton mouseButton) const
{
    const auto it = mouseStateData.find(mouseButton);
    return it != mouseStateData.end() && it->second.released;
}

bool Input::isMouseDown(const MouseButton mouseButton) const
{
    const auto it = mouseStateData.find(mouseButton);
    return it != mouseStateData.end() && it->second.state == InputState::DOWN;
}

Input::InputStateData Input::getKeyData(const Key key) const
{
    const auto it = keyStateData.find(key);
    if (it != keyStateData.end()) {
        return it->second;
    }
    return {};
}

Input::InputStateData Input::getMouseData(const MouseButton mouseButton) const
{
    const auto it = mouseStateData.find(mouseButton);
    if (it != mouseStateData.end()) {
        return it->second;
    }
    return {};
}
}
