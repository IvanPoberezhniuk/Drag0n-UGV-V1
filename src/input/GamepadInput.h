#pragma once
#include "core/AppState.h"
#include <SDL2/SDL.h>

class GamepadInput {
public:
    GamepadInput();
    ~GamepadInput();

    // Process SDL controller connect/disconnect events — call from main event loop.
    void handleEvent(const SDL_Event& e);

    // Poll axis/button state and update ControlState in appState.
    void poll(AppState& appState);

    bool isConnected() const { return m_controller != nullptr; }
    const char* name() const;

private:
    void open(int index);
    void close();

    SDL_GameController* m_controller = nullptr;
    bool m_prevA = false, m_prevB = false, m_prevY = false, m_prevX = false;
    static constexpr float kDeadzone = 0.12f;
};
