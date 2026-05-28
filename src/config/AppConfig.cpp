#include "config/AppConfig.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <spdlog/spdlog.h>

AppConfig AppConfig::defaults() {
    return AppConfig{};
}

AppConfig AppConfig::load(const std::string& path) {
    AppConfig cfg;
    std::ifstream f(path);
    if (!f.is_open()) {
        spdlog::warn("AppConfig: cannot open '{}', using defaults", path);
        return cfg;
    }

    try {
        auto j = nlohmann::json::parse(f);

        if (j.contains("serial")) {
            auto& s = j["serial"];
            if (s.contains("port"))     cfg.serial.port     = s["port"].get<std::string>();
            if (s.contains("baudrate")) cfg.serial.baudrate  = s["baudrate"].get<uint32_t>();
        }
        if (j.contains("control")) {
            auto& c = j["control"];
            if (c.contains("rateHz"))            cfg.control.rateHz            = c["rateHz"].get<uint32_t>();
            if (c.contains("failsafeTimeoutMs")) cfg.control.failsafeTimeoutMs = c["failsafeTimeoutMs"].get<uint32_t>();
        }
        if (j.contains("channels")) {
            auto& ch = j["channels"];
            if (ch.contains("steering")) cfg.channels.steering = ch["steering"].get<int>();
            if (ch.contains("throttle")) cfg.channels.throttle = ch["throttle"].get<int>();
            if (ch.contains("mode"))     cfg.channels.mode     = ch["mode"].get<int>();
            if (ch.contains("lights"))   cfg.channels.lights   = ch["lights"].get<int>();
            if (ch.contains("arm"))      cfg.channels.arm      = ch["arm"].get<int>();
            if (ch.contains("estop"))    cfg.channels.estop    = ch["estop"].get<int>();
        }

        spdlog::info("AppConfig: loaded '{}' — serial={} @{} baud, {}Hz",
            path, cfg.serial.port, cfg.serial.baudrate, cfg.control.rateHz);
    } catch (const std::exception& e) {
        spdlog::error("AppConfig: parse error in '{}': {}", path, e.what());
    }

    return cfg;
}
