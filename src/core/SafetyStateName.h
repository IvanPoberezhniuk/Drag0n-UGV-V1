#pragma once
#include <cstdint>
#include <iterator>

// Names for the STM32 node safety-state machine, as reported in the CRSF
// diagnostic frame's DiagnosticLink::safetyState byte. Shared between
// SerialWorker's debug logging and any UI that surfaces the same value
// (e.g. DashboardBar's STM32 status tooltips) so the two never drift apart.
inline const char* safetyStateName(uint8_t state) {
    static constexpr const char* names[] = {
        "BOOT", "DISABLED", "ARMING", "READY", "ACTIVE",
        "DEGRADED", "FAULT", "ESTOP"
    };
    return state < std::size(names) ? names[state] : "UNKNOWN";
}
