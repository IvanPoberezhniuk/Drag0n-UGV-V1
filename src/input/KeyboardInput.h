#pragma once
#include "input/IInputSource.h"
#include "input/KeyBindings.h"
#include <chrono>
#include <array>

class KeyboardInput : public IInputSource {
public:
    explicit KeyboardInput(const KeyBindings& bindings);
    InputFrame  poll()        override;
    bool        isConnected() const override { return true; }
    const char* name()        const override { return "Keyboard"; }

private:
    const KeyBindings& m_bindings;
    std::array<bool, KeyBindings::Count> m_prevState{};
    bool  m_armed    = false;
    float m_throttle = 0.0f;
    float m_steering = 0.0f;
    std::chrono::steady_clock::time_point m_lastPoll{};
};
