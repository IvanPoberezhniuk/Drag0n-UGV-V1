#pragma once
#include "input/IInputSource.h"

class KeyboardInput : public IInputSource {
public:
    InputFrame  poll()        override;
    bool        isConnected() const override { return true; }
    const char* name()        const override { return "Keyboard"; }

private:
    bool m_prevEnter = false;
    bool m_prevSpace = false;
    bool m_prevL     = false;
    bool m_prev1     = false;
    bool m_prev2     = false;
    bool m_prev3     = false;
    bool m_armed     = false;
};
