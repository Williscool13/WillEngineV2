//
// Created by William on 2024-08-24.
//

#include "input.h"

namespace will_engine
{
    Input::Input()
    {
        keyStateData[SDLK_ESCAPE] = {};
        keyStateData[SDLK_SPACE] = {};
        keyStateData[SDLK_LSHIFT] = {};
        keyStateData[SDLK_LCTRL] = {};
        keyStateData[SDLK_RETURN] = {};
        keyStateData[SDLK_q] = {};
        keyStateData[SDLK_w] = {};
        keyStateData[SDLK_e] = {};
        keyStateData[SDLK_r] = {};
        keyStateData[SDLK_t] = {};
        keyStateData[SDLK_y] = {};
        keyStateData[SDLK_u] = {};
        keyStateData[SDLK_i] = {};
        keyStateData[SDLK_o] = {};
        keyStateData[SDLK_p] = {};
        keyStateData[SDLK_a] = {};
        keyStateData[SDLK_s] = {};
        keyStateData[SDLK_d] = {};
        keyStateData[SDLK_f] = {};
        keyStateData[SDLK_g] = {};
        keyStateData[SDLK_h] = {};
        keyStateData[SDLK_j] = {};
        keyStateData[SDLK_k] = {};
        keyStateData[SDLK_l] = {};
        keyStateData[SDLK_z] = {};
        keyStateData[SDLK_x] = {};
        keyStateData[SDLK_c] = {};
        keyStateData[SDLK_v] = {};
        keyStateData[SDLK_b] = {};
        keyStateData[SDLK_n] = {};
        keyStateData[SDLK_m] = {};
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
        mouseStateData[SDL_BUTTON(4) - 1] = {};
        mouseStateData[SDL_BUTTON(5) - 1] = {};
    }

    void Input::processEvent(const SDL_Event& event)
    {
        switch (event.type) {
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            {
                const auto it = keyStateData.find(event.key.keysym.sym);
                if (it != keyStateData.end()) {
                    UpdateInputState(it->second, event.type == SDL_KEYDOWN);
                }
                break;
            }
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            {
                const auto it = mouseStateData.find(event.button.button - 1);
                if (it != mouseStateData.end()) {
                    UpdateInputState(it->second, event.type == SDL_MOUSEBUTTONDOWN);
                }
                break;
            }
            case SDL_MOUSEMOTION:
            {
                mouseXDelta += static_cast<float>(event.motion.xrel);
                mouseYDelta += static_cast<float>(event.motion.yrel);
                mouseX = static_cast<float>(event.motion.x);
                mouseY = static_cast<float>(event.motion.y);
                break;
            }
            case SDL_MOUSEWHEEL:
            {
                mouseWheelDelta += event.wheel.preciseY;
                break;
            }
            default:
                break;
        }
    }

    void Input::updateFocus(const Uint32 sdlWindowFlags)
    {
        if (windowInputFocus&& isKeyPressed(SDLK_f)) {
            SDL_SetRelativeMouseMode(SDL_GetRelativeMouseMode() == SDL_TRUE ? SDL_FALSE : SDL_TRUE);
        }
        windowInputFocus = (sdlWindowFlags & SDL_WINDOW_INPUT_FOCUS) != 0;
        inFocus = SDL_GetRelativeMouseMode() == SDL_TRUE;
    }

    void Input::frameReset()
    {
        for (std::pair<const int, InputStateData>& key : keyStateData) {
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
