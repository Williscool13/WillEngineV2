//
// Created by William on 2024-08-24.

#ifndef INPUT_H
#define INPUT_H
#include <SDL.h>
#include <SDL_events.h>
#include <unordered_map>
#include <glm/glm.hpp>


namespace will_engine::input
{
enum class Key : uint32_t
{
    // Standard ASCII keys
    UNKNOWN = SDLK_UNKNOWN,
    RETURN = SDLK_RETURN,
    ESCAPE = SDLK_ESCAPE,
    BACKSPACE = SDLK_BACKSPACE,
    TAB = SDLK_TAB,
    SPACE = SDLK_SPACE,
    DELETE = SDLK_DELETE,

    // Number keys
    NUM_0 = SDLK_0,
    NUM_1 = SDLK_1,
    NUM_2 = SDLK_2,
    NUM_3 = SDLK_3,
    NUM_4 = SDLK_4,
    NUM_5 = SDLK_5,
    NUM_6 = SDLK_6,
    NUM_7 = SDLK_7,
    NUM_8 = SDLK_8,
    NUM_9 = SDLK_9,

    // Letter keys
    A = SDLK_A,
    B = SDLK_B,
    C = SDLK_C,
    D = SDLK_D,
    E = SDLK_E,
    F = SDLK_F,
    G = SDLK_G,
    H = SDLK_H,
    I = SDLK_I,
    J = SDLK_J,
    K = SDLK_K,
    L = SDLK_L,
    M = SDLK_M,
    N = SDLK_N,
    O = SDLK_O,
    P = SDLK_P,
    Q = SDLK_Q,
    R = SDLK_R,
    S = SDLK_S,
    T = SDLK_T,
    U = SDLK_U,
    V = SDLK_V,
    W = SDLK_W,
    X = SDLK_X,
    Y = SDLK_Y,
    Z = SDLK_Z,

    // Function keys
    F1 = SDLK_F1,
    F2 = SDLK_F2,
    F3 = SDLK_F3,
    F4 = SDLK_F4,
    F5 = SDLK_F5,
    F6 = SDLK_F6,
    F7 = SDLK_F7,
    F8 = SDLK_F8,
    F9 = SDLK_F9,
    F10 = SDLK_F10,
    F11 = SDLK_F11,
    F12 = SDLK_F12,

    // Arrow keys
    RIGHT = SDLK_RIGHT,
    LEFT = SDLK_LEFT,
    DOWN = SDLK_DOWN,
    UP = SDLK_UP,

    // Modifier keys
    LCTRL = SDLK_LCTRL,
    LSHIFT = SDLK_LSHIFT,
    LALT = SDLK_LALT,
    LGUI = SDLK_LGUI,
    RCTRL = SDLK_RCTRL,
    RSHIFT = SDLK_RSHIFT,
    RALT = SDLK_RALT,
    RGUI = SDLK_RGUI,

    // Navigation keys
    HOME = SDLK_HOME,
    END = SDLK_END,
    PAGEUP = SDLK_PAGEUP,
    PAGEDOWN = SDLK_PAGEDOWN,
    INSERT = SDLK_INSERT,

    // Special keys
    PRINTSCREEN = SDLK_PRINTSCREEN,
    SCROLLLOCK = SDLK_SCROLLLOCK,
    PAUSE = SDLK_PAUSE,
    CAPSLOCK = SDLK_CAPSLOCK,
    NUMLOCKCLEAR = SDLK_NUMLOCKCLEAR,

    // Punctuation
    PERIOD = SDLK_PERIOD,
    COMMA = SDLK_COMMA,
    SEMICOLON = SDLK_SEMICOLON,
    APOSTROPHE = SDLK_APOSTROPHE,
    SLASH = SDLK_SLASH,
    BACKSLASH = SDLK_BACKSLASH,
    MINUS = SDLK_MINUS,
    EQUALS = SDLK_EQUALS,
    LEFTBRACKET = SDLK_LEFTBRACKET,
    RIGHTBRACKET = SDLK_RIGHTBRACKET,
    BACKTICK = SDLK_GRAVE
};

enum class MouseButton : uint8_t
{
    LMB = SDL_BUTTON_LMASK, // 1 << 0 = 0b0001
    RMB = SDL_BUTTON_RMASK, // 1 << 2 = 0b0100
    MMB = SDL_BUTTON_MMASK, // 1 << 1 = 0b0010
    X1 = SDL_BUTTON_X1MASK, // 1 << 3 = 0b1000
    X2 = SDL_BUTTON_X2MASK, // 1 << 4 = 0b10000
};

/**
* The singleton input manager.
* \n Access specific keyboard inputs with SDL_Keycode and mouse inputs with numbers (0 is lmb, 1 is mmb, 2 is rmb)
*/
class Input
{
public:
    static Input& get()
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

private:
    std::unordered_map<Key, InputStateData> keyStateData{
        {Key::UNKNOWN, {}}, {Key::RETURN, {}}, {Key::ESCAPE, {}},
        {Key::BACKSPACE, {}}, {Key::TAB, {}}, {Key::SPACE, {}},
        {Key::DELETE, {}},

        {Key::NUM_0, {}}, {Key::NUM_1, {}}, {Key::NUM_2, {}},
        {Key::NUM_3, {}}, {Key::NUM_4, {}}, {Key::NUM_5, {}},
        {Key::NUM_6, {}}, {Key::NUM_7, {}}, {Key::NUM_8, {}},
        {Key::NUM_9, {}},

        {Key::A, {}}, {Key::B, {}}, {Key::C, {}}, {Key::D, {}},
        {Key::E, {}}, {Key::F, {}}, {Key::G, {}}, {Key::H, {}},
        {Key::I, {}}, {Key::J, {}}, {Key::K, {}}, {Key::L, {}},
        {Key::M, {}}, {Key::N, {}}, {Key::O, {}}, {Key::P, {}},
        {Key::Q, {}}, {Key::R, {}}, {Key::S, {}}, {Key::T, {}},
        {Key::U, {}}, {Key::V, {}}, {Key::W, {}}, {Key::X, {}},
        {Key::Y, {}}, {Key::Z, {}},

        {Key::F1, {}}, {Key::F2, {}}, {Key::F3, {}}, {Key::F4, {}},
        {Key::F5, {}}, {Key::F6, {}}, {Key::F7, {}}, {Key::F8, {}},
        {Key::F9, {}}, {Key::F10, {}}, {Key::F11, {}}, {Key::F12, {}},

        {Key::RIGHT, {}}, {Key::LEFT, {}}, {Key::DOWN, {}}, {Key::UP, {}},

        {Key::LCTRL, {}}, {Key::LSHIFT, {}}, {Key::LALT, {}}, {Key::LGUI, {}},
        {Key::RCTRL, {}}, {Key::RSHIFT, {}}, {Key::RALT, {}}, {Key::RGUI, {}},

        {Key::HOME, {}}, {Key::END, {}}, {Key::PAGEUP, {}},
        {Key::PAGEDOWN, {}}, {Key::INSERT, {}},

        {Key::PRINTSCREEN, {}}, {Key::SCROLLLOCK, {}},
        {Key::PAUSE, {}}, {Key::CAPSLOCK, {}}, {Key::NUMLOCKCLEAR, {}},

        {Key::PERIOD, {}}, {Key::COMMA, {}}, {Key::SEMICOLON, {}},
        {Key::APOSTROPHE, {}}, {Key::SLASH, {}}, {Key::BACKSLASH, {}},
        {Key::MINUS, {}}, {Key::EQUALS, {}}, {Key::LEFTBRACKET, {}},
        {Key::RIGHTBRACKET, {}}, {Key::BACKTICK, {}}
    };

    std::unordered_map<MouseButton, InputStateData> mouseStateData{
        {MouseButton::LMB, {}},
        {MouseButton::RMB, {}},
        {MouseButton::MMB, {}},
        {MouseButton::X1, {}},
        {MouseButton::X2, {}}
    };

public:
    Input() = default;

    void init(SDL_Window* window, uint32_t w, uint32_t h);

    void processEvent(const SDL_Event& event);

    void updateFocus(Uint32 sdlWindowFlags);

    void frameReset();

    bool isKeyPressed(Key key) const;

    bool isKeyReleased(Key key) const;

    bool isKeyDown(Key key) const;

    bool isMousePressed(MouseButton mouseButton) const;

    bool isMouseReleased(MouseButton mouseButton) const;

    bool isMouseDown(MouseButton mouseButton) const;

    InputStateData getKeyData(Key key) const;

    InputStateData getMouseData(MouseButton mouseButton) const;

    void updateWindowExtent(const uint32_t w, const uint32_t h) { this->windowExtents = glm::vec2(static_cast<float>(w), static_cast<float>(h)); }

    glm::vec2 getMousePosition() const { return mousePosition; }
    glm::vec2 getMousePositionAbsolute() const { return mousePositionAbsolute; }
    float getMouseXDelta() const { return mouseXDelta; }
    float getMouseYDelta() const { return mouseYDelta; }
    float getMouseWheelDelta() const { return mouseWheelDelta; }

    bool isTyping() const { return bIsTyping; }
    bool isPopupActive() const { return bIsPopupActive; }
    bool isCursorActive() const { return bIsCursorActive; }
    bool isWindowInputFocus() const { return bIsWindowInputFocus; }

private:
    /**
     * [0,1], [0,1]
     */
    glm::vec2 mousePosition{};
    /**
     * [0,windowExtent.width], [0, windowExtent.height]
     */
    glm::vec2 mousePositionAbsolute{};
    float mouseXDelta{0.0f};
    float mouseYDelta{0.0f};
    float mouseWheelDelta{0.0f};

    bool bIsTyping{false};
    bool bIsPopupActive{false};
    bool bIsCursorActive{false};
    bool bIsWindowInputFocus{false};


    SDL_Window* window{nullptr};
    glm::vec2 windowExtents{1700, 900};

    static void UpdateInputState(InputStateData& inputButton, bool isPressed);
};
}
#endif //INPUT_H
