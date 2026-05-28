#pragma once
#include <cmath>

namespace InputUtils {

inline float smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }

inline float eased(float v) {
    float s = v < 0.0f ? -1.0f : 1.0f;
    return s * smoothstep(std::abs(v));
}

inline float ramp(float cur, float target, float rate, float dt) {
    float diff = target - cur;
    float step = rate * dt;
    if (std::abs(diff) <= step) return target;
    return cur + std::copysign(step, diff);
}

inline float applyDeadzone(float v, float dz) {
    if (std::fabs(v) < dz) return 0.0f;
    return (v - std::copysign(dz, v)) / (1.0f - dz);
}

} // namespace InputUtils
