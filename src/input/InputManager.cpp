#include "input/InputManager.h"
#include "core/ControlState.h"
#include "core/SafetyState.h"
#include <spdlog/spdlog.h>
#include <mutex>
#include <algorithm>

void InputManager::addSource(std::unique_ptr<IInputSource> src) {
    m_sources.push_back({ std::move(src), {} });
}

void InputManager::poll(AppState& state) {
    if (m_sources.empty()) return;

    auto now = std::chrono::steady_clock::now();

    // Collect frames from all sources
    std::vector<InputFrame> frames;
    frames.reserve(m_sources.size());
    for (auto& entry : m_sources) {
        InputFrame f = entry.source->poll();
        if (f.hasAxes)
            entry.lastAxisTime = now;
        frames.push_back(f);
    }

    // Pick axes from source with most recent lastAxisTime
    auto it = std::max_element(m_sources.begin(), m_sources.end(),
        [](const Entry& a, const Entry& b) {
            return a.lastAxisTime < b.lastAxisTime;
        });

    float throttle = 0.0f, steering = 0.0f;
    if (it != m_sources.end() && it->lastAxisTime != std::chrono::steady_clock::time_point{}) {
        const auto& f = frames[static_cast<size_t>(it - m_sources.begin())];
        throttle = f.throttle;
        steering = f.steering;
    }

    // OR all button events
    bool arm = false, disarm = false, estop = false, toggleLights = false;
    int  setDriveMode = 0;
    for (const auto& f : frames) {
        arm          |= f.arm;
        disarm       |= f.disarm;
        estop        |= f.estop;
        toggleLights |= f.toggleLights;
        if (f.setDriveMode > 0) setDriveMode = f.setDriveMode;
    }

    std::lock_guard<std::mutex> lk(state.registryMutex);
    auto& ctrl   = state.registry.get<ControlState>(state.ugv);
    auto& safety = state.registry.get<SafetyState>(state.ugv);

    ctrl.throttle    = throttle;
    ctrl.steering    = steering;
    ctrl.lastUpdated = now;

    if (arm) {
        ctrl.armed          = true;
        ctrl.estop          = false;
        safety.estopLatched = false;
        spdlog::info("Input: ARMED");
    }
    if (disarm) {
        ctrl.armed = false;
        spdlog::info("Input: DISARMED");
    }
    if (estop) {
        ctrl.estop  = true;
        ctrl.armed  = false;
        spdlog::warn("Input: EMERGENCY STOP — re-arm to resume");
    }
    if (toggleLights) {
        ctrl.lightsOn = !ctrl.lightsOn;
        spdlog::info("Input: lights {}", ctrl.lightsOn ? "ON" : "OFF");
    }
    if (setDriveMode > 0) {
        ctrl.driveMode = setDriveMode;
        spdlog::info("Input: drive mode {}", setDriveMode);
    }
}
