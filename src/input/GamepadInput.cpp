#include "input/GamepadInput.h"
#include "core/ControlState.h"
#include "core/SafetyState.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <mutex>

static float applyDeadzone(float v, float dz) {
    if (std::fabs(v) < dz) return 0.0f;
    return (v - std::copysign(dz, v)) / (1.0f - dz);
}

static float axisNorm(Sint16 raw) {
    return raw < 0
        ? static_cast<float>(raw) / 32768.0f
        : static_cast<float>(raw) / 32767.0f;
}

static float triggerNorm(Sint16 raw) {
    return static_cast<float>(raw) / 32767.0f;
}

static bool risingEdge(bool cur, bool& prev) {
    bool edge = cur && !prev;
    prev = cur;
    return edge;
}

GamepadInput::GamepadInput() {
    // Open first available controller on startup
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            open(i);
            break;
        }
    }
}

GamepadInput::~GamepadInput() { close(); }

void GamepadInput::open(int index) {
    m_controller = SDL_GameControllerOpen(index);
    if (m_controller)
        spdlog::info("Gamepad: connected — {}", SDL_GameControllerName(m_controller));
}

void GamepadInput::close() {
    if (m_controller) {
        spdlog::info("Gamepad: disconnected — {}", SDL_GameControllerName(m_controller));
        SDL_GameControllerClose(m_controller);
        m_controller = nullptr;
    }
}

const char* GamepadInput::name() const {
    return m_controller ? SDL_GameControllerName(m_controller) : "None";
}

void GamepadInput::handleEvent(const SDL_Event& e) {
    if (e.type == SDL_CONTROLLERDEVICEADDED) {
        if (!m_controller) open(e.cdevice.which);
    } else if (e.type == SDL_CONTROLLERDEVICEREMOVED) {
        if (m_controller &&
            SDL_GameControllerFromInstanceID(e.cdevice.which) == m_controller)
            close();
    }
}

void GamepadInput::poll(AppState& appState) {
    if (!m_controller) return;

    std::lock_guard<std::mutex> lk(appState.registryMutex);
    auto& ctrl   = appState.registry.get<ControlState>(appState.ugv);
    auto& safety = appState.registry.get<SafetyState>(appState.ugv);

    // Left stick X → steering
    float steering = applyDeadzone(
        axisNorm(SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_LEFTX)),
        kDeadzone);

    // Right trigger → forward throttle, left trigger → braking
    float rt = triggerNorm(SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT));
    float lt = triggerNorm(SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT));
    float throttle = rt - lt;

    ctrl.steering    = std::max(-1.0f, std::min(1.0f, steering));
    ctrl.throttle    = std::max(-1.0f, std::min(1.0f, throttle));
    ctrl.lastUpdated = std::chrono::steady_clock::now();

    // A = arm toggle
    bool btnA = SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_A) != 0;
    if (risingEdge(btnA, m_prevA)) {
        ctrl.armed = !ctrl.armed;
        if (ctrl.armed) {
            ctrl.estop          = false;
            safety.estopLatched = false;
            spdlog::info("Gamepad: ARMED");
        } else {
            spdlog::info("Gamepad: DISARMED");
        }
    }

    // B = ESTOP
    bool btnB = SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_B) != 0;
    if (risingEdge(btnB, m_prevB)) {
        ctrl.estop = true;
        ctrl.armed = false;
        spdlog::warn("Gamepad: EMERGENCY STOP");
    }

    // Y = lights toggle
    bool btnY = SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_Y) != 0;
    if (risingEdge(btnY, m_prevY)) {
        ctrl.lightsOn = !ctrl.lightsOn;
        spdlog::info("Gamepad: lights {}", ctrl.lightsOn ? "ON" : "OFF");
    }

    // X = drive mode cycle 1→2→3→1
    bool btnX = SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_X) != 0;
    if (risingEdge(btnX, m_prevX)) {
        ctrl.driveMode = ctrl.driveMode % 3 + 1;
        spdlog::info("Gamepad: drive mode {}", ctrl.driveMode);
    }
}
