#pragma once
#include <array>
#include <chrono>
#include <cstdint>

struct TelemetryState {
    int   rssi1          = 0;
    int   rssi2          = 0;
    int   lq             = 0;
    float batteryVoltage = 0.0f;
    float batteryCurrent = 0.0f;
    float batteryRemainingAh = 0.0f;
    float batteryFullAh = 0.0f;
    uint8_t batterySocPct = 0;
    uint16_t batteryCycleCount = 0;
    uint16_t batteryCellMinMv = 0;
    uint16_t batteryCellMaxMv = 0;
    uint16_t batteryCellDeltaMv = 0;
    std::array<uint16_t, 4> batteryCellMv{}; // pack is confirmed 4S
    bool batteryCharging = false;
    bool batteryDischarging = false;
    bool batteryChargerPlugged = false;
    uint8_t batteryBalancerStatus = 0; // 0 off, 1 charging balancer, 2 discharging balancer
    int8_t batteryTempLowC = 0;
    int8_t batteryTempHighC = 0;
    uint32_t batteryAlarmBits = 0;
    bool bmsConnected = false;
    bool bmsValid = false;
    std::chrono::steady_clock::time_point bmsLastReceived{};
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

    // Detail fields for the same diagnostic frame, surfaced in the
    // DashboardBar's STM32 status tooltips (see core/SafetyStateName.h for
    // safetyState -> string).
    uint8_t  stmLeftState       = 0;
    uint8_t  stmRightState      = 0;
    uint8_t  stmLeftFaultMask   = 0;
    uint8_t  stmRightFaultMask  = 0;
    uint16_t stmLeftAgeMs       = 0xFFFFu;
    uint16_t stmRightAgeMs      = 0xFFFFu;
    uint32_t stmLeftUptimeMs      = 0;
    uint32_t stmRightUptimeMs     = 0;
    uint16_t stmLeftStackFreeBytes  = 0;
    uint16_t stmRightStackFreeBytes = 0;

    // ESP32 AUX node's own uptime/free heap, from the same diagnostic frame.
    uint32_t espUptimeMs      = 0;
    uint32_t espFreeHeapBytes = 0;
};
