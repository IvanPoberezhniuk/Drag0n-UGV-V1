#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <xinput.h>
#include "input/XInputGamepad.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

XInputGamepad::XInputGamepad(int playerIndex) : m_index(playerIndex) {}

bool XInputGamepad::isConnected() const { return m_connected; }

const char* XInputGamepad::name() const { return "Xbox Controller (XInput)"; }

static float applyDeadzone(float v, float dz) {
    if (std::fabs(v) < dz) return 0.0f;
    return (v - std::copysign(dz, v)) / (1.0f - dz);
}

static bool risingEdge(bool cur, bool& prev) {
    bool edge = cur && !prev;
    prev = cur;
    return edge;
}

InputFrame XInputGamepad::poll() {
    InputFrame f;

    XINPUT_STATE state{};
    bool nowConnected = (XInputGetState(static_cast<DWORD>(m_index), &state) == ERROR_SUCCESS);

    if (nowConnected && !m_connected)
        spdlog::info("XInputGamepad: controller {} connected", m_index);
    else if (!nowConnected && m_connected)
        spdlog::info("XInputGamepad: controller {} disconnected", m_index);
    m_connected = nowConnected;

    if (!m_connected) return f;

    auto& pad = state.Gamepad;

    // Left stick X → steering  (-32768..32767)
    float steerRaw = pad.sThumbLX < 0
        ? static_cast<float>(pad.sThumbLX) / 32768.0f
        : static_cast<float>(pad.sThumbLX) / 32767.0f;
    f.steering = applyDeadzone(steerRaw, kDeadzone);

    // RT - LT → throttle  (0..255)
    float rt = static_cast<float>(pad.bRightTrigger) / 255.0f;
    float lt = static_cast<float>(pad.bLeftTrigger)  / 255.0f;
    if (rt < kTriggerDeadzone) rt = 0.0f;
    if (lt < kTriggerDeadzone) lt = 0.0f;
    f.throttle = std::max(-1.0f, std::min(1.0f, rt - lt));

    f.hasAxes = (f.throttle != 0.0f || f.steering != 0.0f);
    f.steering = std::max(-1.0f, std::min(1.0f, f.steering));

    bool btnA = (pad.wButtons & XINPUT_GAMEPAD_A) != 0;
    bool btnB = (pad.wButtons & XINPUT_GAMEPAD_B) != 0;
    bool btnX = (pad.wButtons & XINPUT_GAMEPAD_X) != 0;
    bool btnY = (pad.wButtons & XINPUT_GAMEPAD_Y) != 0;

    // A = arm/disarm toggle
    if (risingEdge(btnA, m_prevA)) {
        m_armed = !m_armed;
        if (m_armed) f.arm    = true;
        else         f.disarm = true;
    }

    if (risingEdge(btnB, m_prevB)) f.estop        = true;
    if (risingEdge(btnY, m_prevY)) f.toggleLights  = true;
    if (risingEdge(btnX, m_prevX)) {
        m_driveMode = m_driveMode % 3 + 1;
        f.setDriveMode = m_driveMode;
    }

    return f;
}
