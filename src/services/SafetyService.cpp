#include "services/SafetyService.h"
#include <spdlog/spdlog.h>
#include <chrono>

namespace SafetyService {

void apply(ControlState& ctrl, SafetyState& safety, uint32_t failsafeTimeoutMs) {
    using Clock = std::chrono::steady_clock;
    using Ms = std::chrono::milliseconds;

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
}

} // namespace SafetyService
