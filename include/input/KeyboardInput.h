#pragma once
#include "IInputSource.h"

class KeyboardInput : public IInputSource {
public:
    KeyboardInput();
    void poll() override;
    ControlState getState() const override;

private:
    ControlState m_state;
    bool m_prevL    = false;
    bool m_prev1    = false;
    bool m_prev2    = false;
    bool m_prev3    = false;
    bool m_prevArm  = false;
    bool m_prevEstop = false;
};
