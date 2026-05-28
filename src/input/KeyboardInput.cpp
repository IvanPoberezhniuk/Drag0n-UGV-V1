#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "input/KeyboardInput.h"
#include <algorithm>

static bool risingEdge(int vkey, bool& prev) {
    bool cur = (GetAsyncKeyState(vkey) & 0x8000) != 0;
    bool edge = cur && !prev;
    prev = cur;
    return edge;
}

InputFrame KeyboardInput::poll() {
    InputFrame f;

    float throttle = 0.0f, steering = 0.0f;
    if (GetAsyncKeyState('W') & 0x8000) throttle += 1.0f;
    if (GetAsyncKeyState('S') & 0x8000) throttle -= 1.0f;
    if (GetAsyncKeyState('D') & 0x8000) steering += 1.0f;
    if (GetAsyncKeyState('A') & 0x8000) steering -= 1.0f;

    f.throttle = std::max(-1.0f, std::min(1.0f, throttle));
    f.steering = std::max(-1.0f, std::min(1.0f, steering));
    f.hasAxes  = (throttle != 0.0f || steering != 0.0f);

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
