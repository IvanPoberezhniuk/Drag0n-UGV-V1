#pragma once
#include "input/IInputSource.h"
#include "input/KeyBindings.h"
#include "input/EdgeDetector.h"
#include "config/AppConfig.h"
#include <chrono>

class KeyboardInput : public IInputSource {
public:
    explicit KeyboardInput(const KeyBindings& bindings, const AppConfig::InputCfg& cfg);
    InputFrame  poll()        override;
    bool        isConnected() const override { return true; }
    const char* name()        const override { return "Keyboard"; }

private:
    const KeyBindings&         m_bindings;
    const AppConfig::InputCfg& m_cfg;
    EdgeDetectorArray<KeyBindings::Count> m_edge;
    bool  m_armed    = false;
    float m_throttle = 0.0f;
    float m_steering = 0.0f;
    std::chrono::steady_clock::time_point m_lastPoll{};
};
