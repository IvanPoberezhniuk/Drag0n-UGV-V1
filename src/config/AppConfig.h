#pragma once
#include <string>
#include <cstdint>

struct AppConfig {
    struct SerialCfg {
        std::string port             = "auto";
        uint32_t    baudrate         = 420000;
        uint32_t    reconnectDelayMs = 2000;
    } serial;

    struct ControlCfg {
        uint32_t rateHz              = 50;
        uint32_t failsafeTimeoutMs   = 300;
        uint32_t telemetryTimeoutMs  = 5000;
        uint32_t writeErrorThreshold = 5;
    } control;

    struct ChannelsCfg {
        int steering = 1;
        int throttle = 2;
        int mode     = 3;
        int lights   = 4;
        int arm      = 5;
        int estop    = 6;
    } channels;

    struct UiCfg {
        int      fontSize          = 12;
        uint32_t refreshIntervalMs = 30;
    } ui;

    struct InputCfg {
        float stickDeadzone   = 0.12f;
        float triggerDeadzone = 0.05f;
        float keyAccel        = 5.0f;
        float keyDecel        = 10.0f;
    } input;

    static AppConfig load(const std::string& path);
    static AppConfig defaults();
};
