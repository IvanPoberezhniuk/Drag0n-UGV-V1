#pragma once
#include <array>
#include <chrono>

struct TelemetryState {
    int   rssi1          = 0;
    int   rssi2          = 0;
    int   lq             = 0;
    float batteryVoltage = 0.0f;
    float speed          = 0.0f;
    float heading        = 0.0f;
    std::array<int, 6>  motorRpm{};
    std::array<bool, 6> motorRpmValid{};
    bool  valid          = false;
    std::chrono::steady_clock::time_point lastReceived{};
    std::array<std::chrono::steady_clock::time_point, 6> motorRpmLastReceived{};

    // STM32 node link health, decoded from the ESP32's CRSF diagnostic frame.
    bool stmLeftOnline  = false;
    bool stmRightOnline = false;
    std::chrono::steady_clock::time_point diagnosticLastReceived{};
};
