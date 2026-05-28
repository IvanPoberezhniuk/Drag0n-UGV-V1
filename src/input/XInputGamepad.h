#pragma once
#include "input/IInputSource.h"

class XInputGamepad : public IInputSource {
public:
    explicit XInputGamepad(int playerIndex = 0);
    InputFrame  poll()        override;
    bool        isConnected() const override;
    const char* name()        const override;

private:
    int  m_index;
    bool m_connected = false;
    bool m_prevA = false, m_prevB = false, m_prevX = false, m_prevY = false;
    bool m_armed = false;
    int  m_driveMode = 1;
    static constexpr float kDeadzone = 0.12f;
    static constexpr float kTriggerDeadzone = 0.05f;
};
