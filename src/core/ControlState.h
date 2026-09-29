#pragma once
#include <chrono>

// RC-channel drive mode selection. Values are wire-significant: ControlService
// maps them 2WD->CH_MIN, 4WD->CH_CENTER, 6WD->CH_MAX (see ControlService.cpp).
enum class DriveMode { TwoWD = 1, FourWD = 2, SixWD = 3 };

// RC-channel turn mode selection. Values are wire-significant: ControlService
// maps them AllWheel->CH_MIN, CenterPivot->CH_MAX (see ControlService.cpp).
// CenterPivot applies the steering differential only to the center-axle
// wheels (front/rear roll at plain throttle), reducing scrub on long 6WD
// turns -- mixed on the ESP32, see firmware/esp32 ugv_manual_control.c.
enum class TurnMode { AllWheel = 1, CenterPivot = 2 };

struct ControlState {
    float     throttle  = 0.0f;
    float     steering  = 0.0f;
    bool      armed     = false;
    bool      estop     = false;
    bool      clearFault = false;
    std::chrono::steady_clock::time_point clearFaultSetAt{};
    bool      lightsOn  = false;
    bool      cruiseEnabled = false;
    float     cruiseSpeed   = 0.5f; // fraction of full throttle, adjustable in 5% steps
    DriveMode driveMode = DriveMode::TwoWD;
    TurnMode  turnMode  = TurnMode::AllWheel;
    std::chrono::steady_clock::time_point lastUpdated{};
};
