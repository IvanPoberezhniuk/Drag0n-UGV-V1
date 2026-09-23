#include "services/SafetyService.h"
#include "core/ChronoTypes.h"
#include <spdlog/spdlog.h>

namespace SafetyService {

void apply(ControlState& ctrl, SafetyState& safety, uint32_t failsafeTimeoutMs) {
    auto now = Clock::now();
    auto msSince = std::chrono::duration_cast<Ms>(now - ctrl.lastUpdated).count();

    // Input timeout failsafe
    if (msSince > static_cast<long long>(failsafeTimeoutMs)) {
        if (!safety.failsafeActive) {
            spdlog::warn("Safety: no input for {}ms — holding neutral", msSince);
            safety.failsafeActive = true;
        }
        ctrl.throttle = 0.0f;
        ctrl.steering = 0.0f;
    } else {
        safety.failsafeActive = false;
    }

    // Armed interlock: disarmed always means zero throttle
    if (!ctrl.armed) {
        ctrl.throttle = 0.0f;
    }

    // ESTOP latch: once triggered, clears only via explicit re-arm
    if (ctrl.estop) {
        safety.estopLatched = true;
    }
    if (safety.estopLatched) {
        ctrl.throttle = 0.0f;
        ctrl.steering = 0.0f;
        ctrl.armed    = false;
    }

    // Connection lost failsafe
    if (safety.connectionLost) {
        ctrl.throttle = 0.0f;
        ctrl.steering = 0.0f;
        ctrl.armed    = false;
    }

    // Clear-fault is a one-shot pulse on its RC channel: hold it high just
    // long enough for ESP32/STM32 to see it, then drop it automatically so
    // it can't sit high and block a later ARM (see uart_control_service.c's
    // CLEAR_FAULT branch, which takes priority over ARM in the same frame).
    if (ctrl.clearFault) {
        ctrl.throttle = 0.0f;
        ctrl.steering = 0.0f;
        if (std::chrono::duration_cast<Ms>(now - ctrl.clearFaultSetAt).count() > 250) {
            ctrl.clearFault = false;
        }
    }
}

} // namespace SafetyService
