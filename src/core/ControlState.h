#pragma once
#include <chrono>

struct ControlState {
    float throttle  = 0.0f;
    float steering  = 0.0f;
    bool  armed     = false;
    bool  estop     = false;
    bool  lightsOn  = false;
    int   driveMode = 1;
    std::chrono::steady_clock::time_point lastUpdated{};
};
