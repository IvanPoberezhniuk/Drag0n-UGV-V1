#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "input/KeyboardInput.h"
#include <algorithm>
#include <cmath>

static bool risingEdge(int vkey, bool& prev) {
    bool cur = (GetAsyncKeyState(vkey) & 0x8000) != 0;
    bool edge = cur && !prev;
    prev = cur;
    return edge;
}

// Cubic ease-in/out: maps 0..1 → 0..1 with smooth acceleration and deceleration
static float smoothstep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

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

InputFrame KeyboardInput::poll() {
    InputFrame f;

    auto now = std::chrono::steady_clock::now();
    float dt = 0.033f;
    if (m_lastPoll != std::chrono::steady_clock::time_point{})
        dt = std::min(std::chrono::duration<float>(now - m_lastPoll).count(), 0.1f);
    m_lastPoll = now;

    float targetThrottle = 0.0f, targetSteering = 0.0f;
    if (GetAsyncKeyState('W') & 0x8000) targetThrottle += 1.0f;
    if (GetAsyncKeyState('S') & 0x8000) targetThrottle -= 1.0f;
    if (GetAsyncKeyState('D') & 0x8000) targetSteering += 1.0f;
    if (GetAsyncKeyState('A') & 0x8000) targetSteering -= 1.0f;

    // 5.0 = full deflection in ~200ms, 10.0 = return to zero in ~100ms
    const float kAccel = 5.0f;
    const float kDecel = 10.0f;

    m_throttle = ramp(m_throttle, targetThrottle, targetThrottle == 0.0f ? kDecel : kAccel, dt);
    m_steering = ramp(m_steering, targetSteering, targetSteering == 0.0f ? kDecel : kAccel, dt);

    f.throttle = eased(m_throttle);
    f.steering = eased(m_steering);
    f.hasAxes  = (m_throttle != 0.0f || m_steering != 0.0f);

    if (risingEdge(VK_RETURN, m_prevEnter)) {
        m_armed = !m_armed;
        if (m_armed) f.arm    = true;
        else         f.disarm = true;
    }

    if (risingEdge(VK_SPACE, m_prevSpace)) f.estop        = true;
    if (risingEdge('L',      m_prevL))     f.toggleLights = true;
    if (risingEdge('1',      m_prev1))     f.setDriveMode = 1;
    if (risingEdge('2',      m_prev2))     f.setDriveMode = 2;
    if (risingEdge('3',      m_prev3))     f.setDriveMode = 3;

    return f;
}
