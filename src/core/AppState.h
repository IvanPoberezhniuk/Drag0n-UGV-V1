#pragma once
#include <mutex>
#include <atomic>
#include <entt/entt.hpp>
#include "core/ControlState.h"
#include "core/TelemetryState.h"
#include "core/SafetyState.h"
#include "core/ConnectionState.h"
#include "core/Events.h"
#include "core/LogBuffer.h"
#include "input/KeyBindings.h"

enum class InputType { Keyboard, Gamepad };

struct AppState {
    entt::registry   registry;
    entt::dispatcher dispatcher;
    entt::entity     ugv{entt::null};

    std::mutex registryMutex;

    std::atomic<bool> quit{false};

    InputType          activeInput      = InputType::Keyboard;
    std::atomic<int>   wheelSizePercent {100};
    std::atomic<bool>  whiteNoiseEnabled {true};

    // Dashboard status-icon click-to-toggle state (see DashboardBar). Camera
    // has real effect (pauses/resumes VideoWorker); GPS/velocity/speaker have
    // no telemetry source behind them at all yet, so their toggle is
    // UI-preference-only. They default enabled so missing hardware is shown
    // as plain gray; a cross appears only after an explicit user disable.
    std::atomic<bool> cameraEnabled        {true};
    std::atomic<bool> gpsWatchEnabled      {true};
    std::atomic<bool> velocityWatchEnabled {true};
    std::atomic<bool> speakerWatchEnabled  {true};

    LogBuffer logs;

    KeyBindings keyBindings;

    // Serial baud rate: hardcoded to config.json's default unless the user
    // overrides it in Settings > Connection, following the same
    // read-on-UI-thread / edited-in-SettingsDialog convention as
    // keyBindings above.
    uint32_t serialBaudrate = 400000;
};
