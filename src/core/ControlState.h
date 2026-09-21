#pragma once
#include <chrono>

struct ControlState {
    float throttle  = 0.0f;
    float steering  = 0.0f;
    bool  armed     = false;
    bool  estop     = false;
    bool  lightsOn  = false;
    bool  cruiseEnabled = false;
    float cruiseSpeed   = 0.5f; // fraction of full throttle, adjustable in 5% steps
    int   driveMode = 1;
    std::chrono::steady_clock::time_point lastUpdated{};
};
