#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <xinput.h>
#include "input/XInputGamepad.h"
#include "input/InputUtils.h"
#include <spdlog/spdlog.h>
#include <algorithm>

XInputGamepad::XInputGamepad(int playerIndex, const AppConfig::InputCfg& cfg)
    : m_index(playerIndex), m_cfg(cfg) {}

bool XInputGamepad::isConnected() const { return m_connected; }

const char* XInputGamepad::name() const { return "Xbox Controller (XInput)"; }

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
    f.steering = InputUtils::applyDeadzone(steerRaw, m_cfg.stickDeadzone);

    // RT - LT → throttle  (0..255)
    float rt = static_cast<float>(pad.bRightTrigger) / 255.0f;
    float lt = static_cast<float>(pad.bLeftTrigger)  / 255.0f;
    if (rt < m_cfg.triggerDeadzone) rt = 0.0f;
    if (lt < m_cfg.triggerDeadzone) lt = 0.0f;
    f.throttle = std::max(-1.0f, std::min(1.0f, rt - lt));

    f.hasAxes  = (f.throttle != 0.0f || f.steering != 0.0f);
    f.steering = std::max(-1.0f, std::min(1.0f, f.steering));

    bool btnA = (pad.wButtons & XINPUT_GAMEPAD_A) != 0;
    bool btnB = (pad.wButtons & XINPUT_GAMEPAD_B) != 0;
    bool btnY = (pad.wButtons & XINPUT_GAMEPAD_Y) != 0;
    bool btnX = (pad.wButtons & XINPUT_GAMEPAD_X) != 0;

    // A = arm/disarm toggle
    if (m_btnEdge[0].rising(btnA)) {
        m_armed = !m_armed;
        if (m_armed) f.arm    = true;
        else         f.disarm = true;
    }

    if (m_btnEdge[1].rising(btnB)) f.estop       = true;
    if (m_btnEdge[2].rising(btnY)) f.toggleLights = true;
    if (m_btnEdge[3].rising(btnX)) {
        m_driveMode = m_driveMode % 3 + 1;
        f.setDriveMode = m_driveMode;
    }

    return f;
}
