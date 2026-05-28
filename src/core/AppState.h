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

enum class InputType { Keyboard, Gamepad };

// Central application state.
// ECS registry stores UGV components on a single entity.
// Dispatcher (main-thread only) routes typed events to registered handlers.
// registryMutex guards registry access across the main thread and serial worker.
struct AppState {
    entt::registry   registry;
    entt::dispatcher dispatcher;
    entt::entity     ugv{entt::null};

    std::mutex registryMutex;

    std::atomic<bool> quit{false};

    InputType activeInput = InputType::Keyboard;
    LogBuffer logs;
};
