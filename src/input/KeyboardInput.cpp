#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "input/KeyboardInput.h"
#include "input/InputUtils.h"
#include <algorithm>

static int qtKeyToVk(int k) {
    if (k == 0) return 0;
    if (k >= 'A' && k <= 'Z') return k;
    if (k >= '0' && k <= '9') return k;
    switch (k) {
        case 0x20:       return VK_SPACE;
        case 0x01000004: return VK_RETURN;
        case 0x01000005: return VK_RETURN;
        case 0x01000000: return VK_ESCAPE;
        case 0x01000001: return VK_TAB;
        case 0x01000011: return VK_END;
        case 0x01000012: return VK_LEFT;
        case 0x01000013: return VK_UP;
        case 0x01000014: return VK_RIGHT;
        case 0x01000015: return VK_DOWN;
        default:         return 0;
    }
}

static bool vkDown(int qtKey) {
    int vk = qtKeyToVk(qtKey);
    return vk != 0 && (GetAsyncKeyState(vk) & 0x8000) != 0;
}

static bool isDown(const ActionBinding& b) {
    return vkDown(b.key1) || vkDown(b.key2);
}

static bool appHasForegroundWindow() {
    const HWND foreground = GetForegroundWindow();
    if (foreground == nullptr) return false;

    DWORD foregroundProcessId = 0;
    GetWindowThreadProcessId(foreground, &foregroundProcessId);
    return foregroundProcessId == GetCurrentProcessId();
}

KeyboardInput::KeyboardInput(const KeyBindings& bindings, const AppConfig::InputCfg& cfg)
    : m_bindings(bindings), m_cfg(cfg) {}

InputFrame KeyboardInput::poll() {
    InputFrame f;
    using KB = KeyBindings;

    auto now = std::chrono::steady_clock::now();
    float dt = 0.033f;
    if (m_lastPoll != std::chrono::steady_clock::time_point{})
        dt = std::min(std::chrono::duration<float>(now - m_lastPoll).count(), 0.1f);
    m_lastPoll = now;

    const auto& b = m_bindings.actions;

    if (!appHasForegroundWindow()) {
        // GetAsyncKeyState is global. Keep edge state synchronized while another
        // application is focused so a key held there cannot fire on refocus.
        for (size_t i = 0; i < KB::Count; ++i)
            (void)m_edge.rising(i, isDown(b[i]));

        // Focus loss must stop keyboard motion immediately, without a ramp.
        m_throttle = 0.0f;
        m_steering = 0.0f;
        return f;
    }

    // Only a fresh key press cancels cruise -- a WASD key already held when
    // cruise was engaged (and simply released later) must not cancel it.
    bool wasdPressed = false;
    wasdPressed |= m_edge.rising(KB::ThrottleForward,  isDown(b[KB::ThrottleForward]));
    wasdPressed |= m_edge.rising(KB::ThrottleBackward, isDown(b[KB::ThrottleBackward]));
    wasdPressed |= m_edge.rising(KB::SteerLeft,        isDown(b[KB::SteerLeft]));
    wasdPressed |= m_edge.rising(KB::SteerRight,       isDown(b[KB::SteerRight]));
    f.manualOverride = wasdPressed;

    float targetThrottle = 0.0f, targetSteering = 0.0f;
    if (isDown(b[KB::ThrottleForward]))  targetThrottle += 1.0f;
    if (isDown(b[KB::ThrottleBackward])) targetThrottle -= 1.0f;
    if (isDown(b[KB::SteerRight]))       targetSteering += 1.0f;
    if (isDown(b[KB::SteerLeft]))        targetSteering -= 1.0f;

    m_throttle = InputUtils::ramp(m_throttle, targetThrottle,
                                  targetThrottle == 0.0f ? m_cfg.keyDecel : m_cfg.keyAccel, dt);
    m_steering = InputUtils::ramp(m_steering, targetSteering,
                                  targetSteering == 0.0f ? m_cfg.keyDecel : m_cfg.keyAccel, dt);

    f.throttle = InputUtils::eased(m_throttle);
    f.steering = InputUtils::eased(m_steering);
    f.hasAxes  = (m_throttle != 0.0f || m_steering != 0.0f);

    if (m_edge.rising(KB::ArmDisarm, isDown(b[KB::ArmDisarm]))) {
        m_armed = !m_armed;
        if (m_armed) f.arm    = true;
        else         f.disarm = true;
    }
    if (m_edge.rising(KB::EStop,        isDown(b[KB::EStop])))        f.estop        = true;
    if (m_edge.rising(KB::ToggleLights, isDown(b[KB::ToggleLights]))) f.toggleLights = true;
    if (m_edge.rising(KB::DriveMode1,   isDown(b[KB::DriveMode1])))   f.setDriveMode = 1;
    if (m_edge.rising(KB::DriveMode2,   isDown(b[KB::DriveMode2])))   f.setDriveMode = 2;
    if (m_edge.rising(KB::DriveMode3,   isDown(b[KB::DriveMode3])))   f.setDriveMode = 3;

    if (m_edge.rising(KB::ToggleCruise,   isDown(b[KB::ToggleCruise])))   f.toggleCruise = true;
    if (m_edge.rising(KB::CruiseIncrease, isDown(b[KB::CruiseIncrease]))) f.cruiseAdjust = +1;
    if (m_edge.rising(KB::CruiseDecrease, isDown(b[KB::CruiseDecrease]))) f.cruiseAdjust = -1;

    return f;
}
