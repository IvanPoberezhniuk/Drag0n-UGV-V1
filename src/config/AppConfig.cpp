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
            if (s.contains("port"))             cfg.serial.port             = s["port"].get<std::string>();
            if (s.contains("baudrate"))         cfg.serial.baudrate         = s["baudrate"].get<uint32_t>();
            if (s.contains("reconnectDelayMs")) cfg.serial.reconnectDelayMs = s["reconnectDelayMs"].get<uint32_t>();
        }
        if (j.contains("control")) {
            auto& c = j["control"];
            if (c.contains("rateHz"))              cfg.control.rateHz              = c["rateHz"].get<uint32_t>();
            if (c.contains("failsafeTimeoutMs"))   cfg.control.failsafeTimeoutMs   = c["failsafeTimeoutMs"].get<uint32_t>();
            if (c.contains("telemetryTimeoutMs"))  cfg.control.telemetryTimeoutMs  = c["telemetryTimeoutMs"].get<uint32_t>();
            if (c.contains("writeErrorThreshold")) cfg.control.writeErrorThreshold = c["writeErrorThreshold"].get<uint32_t>();
        }
        if (j.contains("channels")) {
            auto& ch = j["channels"];
            auto loadCh = [&](const char* key, int& field) {
                if (!ch.contains(key)) return;
                int v = ch[key].get<int>();
                if (v >= 1 && v <= 16) {
                    field = v;
                } else {
                    spdlog::warn("AppConfig: channel '{}' = {} out of range [1-16], keeping default {}",
                                 key, v, field);
                }
            };
            loadCh("steering", cfg.channels.steering);
            loadCh("throttle", cfg.channels.throttle);
            loadCh("mode",     cfg.channels.mode);
            loadCh("lights",   cfg.channels.lights);
            loadCh("arm",      cfg.channels.arm);
            loadCh("estop",    cfg.channels.estop);
            loadCh("clearFault", cfg.channels.clearFault);
        }

        if (j.contains("ui")) {
            auto& u = j["ui"];
            if (u.contains("fontSize"))          cfg.ui.fontSize          = u["fontSize"].get<int>();
            if (u.contains("refreshIntervalMs")) cfg.ui.refreshIntervalMs = u["refreshIntervalMs"].get<uint32_t>();
        }

        if (j.contains("input")) {
            auto& inp = j["input"];
            if (inp.contains("stickDeadzone"))   cfg.input.stickDeadzone   = inp["stickDeadzone"].get<float>();
            if (inp.contains("triggerDeadzone")) cfg.input.triggerDeadzone = inp["triggerDeadzone"].get<float>();
            if (inp.contains("keyAccel"))        cfg.input.keyAccel        = inp["keyAccel"].get<float>();
            if (inp.contains("keyDecel"))        cfg.input.keyDecel        = inp["keyDecel"].get<float>();
        }

        if (j.contains("video")) {
            auto& v = j["video"];
            if (v.contains("enabled"))          cfg.video.enabled          = v["enabled"].get<bool>();
            if (v.contains("url"))              cfg.video.url              = v["url"].get<std::string>();
            if (v.contains("reconnectDelayMs")) cfg.video.reconnectDelayMs = v["reconnectDelayMs"].get<uint32_t>();
            if (v.contains("openTimeoutMs"))    cfg.video.openTimeoutMs    = v["openTimeoutMs"].get<uint32_t>();
            if (v.contains("staleFrameMs"))     cfg.video.staleFrameMs     = v["staleFrameMs"].get<uint32_t>();
            if (v.contains("preferTcp"))        cfg.video.preferTcp        = v["preferTcp"].get<bool>();
        }

        spdlog::info("AppConfig: loaded '{}' — serial={} @{} baud, {}Hz",
            path, cfg.serial.port, cfg.serial.baudrate, cfg.control.rateHz);
    } catch (const std::exception& e) {
        spdlog::error("AppConfig: parse error in '{}': {}", path, e.what());
    }

    return cfg;
}
