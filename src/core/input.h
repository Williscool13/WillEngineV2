//
// Created by William on 2024-08-24.

#ifndef INPUT_H
#define INPUT_H
#include <SDL_events.h>
#include <unordered_map>


/**
 * THe singleton input manager.
 * \n Access specific keyboard inputs with SDL_Keycode and mouse inputs with numbers (0 is lmb, 1 is mmb, 2 is rmb)
 */
class Input
{
public:
    static Input &Get()
    {
        static Input instance{};
        return instance;
    }

    enum class InputState : bool
    {
        UP = false,
        DOWN = true
    };

    struct InputStateData
    {
        InputState state;
        bool pressed;
        bool released;
    };

    Input();

    void processEvent(const SDL_Event& event);

    void updateFocus(Uint32 sdlWindowFlags);

    void frameReset();

    bool isKeyPressed(SDL_Keycode key) const;

    bool isKeyReleased(SDL_Keycode key) const;

    bool isKeyDown(SDL_Keycode key) const;

    bool isMousePressed(uint8_t mouseButton) const;

    bool isMouseReleased(uint8_t mouseButton) const;

    bool isMouseDown(uint8_t mouseButton) const;

    InputStateData getKeyData(SDL_Keycode key) const;

    InputStateData getMouseData(uint8_t mouseButton) const;


    float getMouseX() const { return mouseX; }
    float getMouseY() const { return mouseY; }
    float getMouseDeltaX() const { return mouseDeltaX; }
    float getMouseDeltaY() const { return mouseDeltaY; }

    bool isInFocus() const { return inFocus; }

private:
    std::unordered_map<SDL_Keycode, InputStateData> keyStateData;
    std::unordered_map<uint8_t, InputStateData> mouseStateData;

    float mouseX{0.0f};
    float mouseY{0.0f};
    float mouseDeltaX{0.0f};
    float mouseDeltaY{0.0f};
    bool inFocus{false};

    void UpdateInputState(InputStateData& inputButton, bool isPressed);
};

#endif //INPUT_H
