#pragma once
#include "core/ControlState.h"
#include "core/SafetyState.h"

namespace SafetyService {

// Applies safety rules to ctrl in-place.
// Modifies safety to reflect current state.
// failsafeTimeoutMs: time without input that triggers neutral hold.
void apply(ControlState& ctrl, SafetyState& safety, uint32_t failsafeTimeoutMs);

} // namespace SafetyService
