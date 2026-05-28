#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "input/KeyboardInput.h"
#include "core/SafetyState.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>

static bool risingEdge(int vkey, bool& prev) {
    bool cur = (GetAsyncKeyState(vkey) & 0x8000) != 0;
    bool edge = cur && !prev;
    prev = cur;
    return edge;
}

void KeyboardInput::poll(AppState& appState) {
    std::lock_guard<std::mutex> lk(appState.registryMutex);
    auto& ctrl   = appState.registry.get<ControlState>(appState.ugv);
    auto& safety = appState.registry.get<SafetyState>(appState.ugv);

    // Throttle (W/S) and steering (A/D) — held keys
    float throttle = 0.0f, steering = 0.0f;
    if (GetAsyncKeyState('W') & 0x8000) throttle += 1.0f;
    if (GetAsyncKeyState('S') & 0x8000) throttle -= 1.0f;
    if (GetAsyncKeyState('D') & 0x8000) steering += 1.0f;
    if (GetAsyncKeyState('A') & 0x8000) steering -= 1.0f;
    ctrl.throttle = std::max(-1.0f, std::min(1.0f, throttle));
    ctrl.steering = std::max(-1.0f, std::min(1.0f, steering));
    ctrl.lastUpdated = std::chrono::steady_clock::now();

    // Arm toggle on Enter — also clears estop latch when re-arming
    if (risingEdge(VK_RETURN, m_prevEnter)) {
        ctrl.armed = !ctrl.armed;
        if (ctrl.armed) {
            ctrl.estop         = false;
            safety.estopLatched = false;
            spdlog::info("Input: ARMED");
        } else {
            spdlog::info("Input: DISARMED");
        }
    }

    // ESTOP on Space — latches
    if (risingEdge(VK_SPACE, m_prevSpace)) {
        ctrl.estop  = true;
        ctrl.armed  = false;
        spdlog::warn("Input: EMERGENCY STOP — re-arm to resume");
    }

    // Lights toggle on L
    if (risingEdge('L', m_prevL)) {
        ctrl.lightsOn = !ctrl.lightsOn;
        spdlog::info("Input: lights {}", ctrl.lightsOn ? "ON" : "OFF");
    }

    // Drive mode 1/2/3
    if (risingEdge('1', m_prev1)) { ctrl.driveMode = 1; spdlog::info("Input: drive mode 1"); }
    if (risingEdge('2', m_prev2)) { ctrl.driveMode = 2; spdlog::info("Input: drive mode 2"); }
    if (risingEdge('3', m_prev3)) { ctrl.driveMode = 3; spdlog::info("Input: drive mode 3"); }
}
