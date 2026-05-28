#pragma once
#include <chrono>

struct TelemetryState {
    int   rssi1          = 0;
    int   rssi2          = 0;
    int   lq             = 0;
    float batteryVoltage = 0.0f;
    float speed          = 0.0f;
    float heading        = 0.0f;
    bool  valid          = false;
    std::chrono::steady_clock::time_point lastReceived{};
};
