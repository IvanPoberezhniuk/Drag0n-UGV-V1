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
    LogBuffer logs;

    KeyBindings keyBindings;
};
