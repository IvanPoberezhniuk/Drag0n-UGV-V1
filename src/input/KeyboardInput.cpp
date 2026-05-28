#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "input/KeyboardInput.h"
#include <algorithm>
#include <cmath>

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

static float smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }

static float eased(float v) {
    float s = v < 0.0f ? -1.0f : 1.0f;
    return s * smoothstep(std::abs(v));
}

static float ramp(float cur, float target, float rate, float dt) {
    float diff = target - cur;
    float step = rate * dt;
    if (std::abs(diff) <= step) return target;
    return cur + std::copysign(step, diff);
}

KeyboardInput::KeyboardInput(const KeyBindings& bindings)
    : m_bindings(bindings) {}

InputFrame KeyboardInput::poll() {
    InputFrame f;
    using KB = KeyBindings;

    auto now = std::chrono::steady_clock::now();
    float dt = 0.033f;
    if (m_lastPoll != std::chrono::steady_clock::time_point{})
        dt = std::min(std::chrono::duration<float>(now - m_lastPoll).count(), 0.1f);
    m_lastPoll = now;

    const auto& b = m_bindings.actions;

    float targetThrottle = 0.0f, targetSteering = 0.0f;
    if (isDown(b[KB::ThrottleForward]))  targetThrottle += 1.0f;
    if (isDown(b[KB::ThrottleBackward])) targetThrottle -= 1.0f;
    if (isDown(b[KB::SteerRight]))       targetSteering += 1.0f;
    if (isDown(b[KB::SteerLeft]))        targetSteering -= 1.0f;

    // 5.0 = full deflection in ~200ms, 10.0 = return to zero in ~100ms
    const float kAccel = 5.0f;
    const float kDecel = 10.0f;
    m_throttle = ramp(m_throttle, targetThrottle, targetThrottle == 0.0f ? kDecel : kAccel, dt);
    m_steering = ramp(m_steering, targetSteering, targetSteering == 0.0f ? kDecel : kAccel, dt);

    f.throttle = eased(m_throttle);
    f.steering = eased(m_steering);
    f.hasAxes  = (m_throttle != 0.0f || m_steering != 0.0f);

    auto rising = [&](int action) -> bool {
        bool cur  = isDown(b[action]);
        bool edge = cur && !m_prevState[action];
        m_prevState[action] = cur;
        return edge;
    };

    if (rising(KB::ArmDisarm)) {
        m_armed = !m_armed;
        if (m_armed) f.arm    = true;
        else         f.disarm = true;
    }
    if (rising(KB::EStop))        f.estop        = true;
    if (rising(KB::ToggleLights)) f.toggleLights = true;
    if (rising(KB::DriveMode1))   f.setDriveMode = 1;
    if (rising(KB::DriveMode2))   f.setDriveMode = 2;
    if (rising(KB::DriveMode3))   f.setDriveMode = 3;

    return f;
}
