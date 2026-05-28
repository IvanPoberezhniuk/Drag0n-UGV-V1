#pragma once
#include "core/AppState.h"

class KeyboardInput {
public:
    // Poll keyboard state and update the ControlState component in appState.
    // Call once per frame from the main thread.
    void poll(AppState& appState);

private:
    bool m_prevEnter = false;
    bool m_prevSpace = false;
    bool m_prevL     = false;
    bool m_prev1     = false;
    bool m_prev2     = false;
    bool m_prev3     = false;
};
