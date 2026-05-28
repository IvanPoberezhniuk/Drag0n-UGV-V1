#pragma once
#include <chrono>

struct ControlState {
    float throttle   = 0.0f;   // -1.0 .. +1.0
    float steering   = 0.0f;   // -1.0 .. +1.0
    bool  armed      = false;
    bool  emergencyStop = false;
    bool  lightsOn   = false;
    int   driveMode  = 1;      // 1, 2, or 3
    std::chrono::steady_clock::time_point lastUpdated{};
};

class IInputSource {
public:
    virtual ~IInputSource() = default;
    virtual void poll() = 0;
    virtual ControlState getState() const = 0;
};
