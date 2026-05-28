#pragma once
#include <string>
#include <cstdint>

struct AppConfig {
    std::string serialPort       = "COM5";
    uint32_t    baudrate         = 420000;
    uint32_t    controlRateHz    = 50;
    uint32_t    failsafeTimeoutMs = 300;
    struct {
        int steering = 1;
        int throttle = 2;
        int mode     = 3;
        int lights   = 4;
        int arm      = 5;
        int estop    = 6;
    } channels;
};

AppConfig loadConfig(const std::string& path);
