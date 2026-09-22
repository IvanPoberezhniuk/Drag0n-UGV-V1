#pragma once
#include <cstdint>
#include <string>

// Camera/video-feed connection status, published by VideoWorker into
// AppState's component registry (under AppState::registryMutex) on
// connect/disconnect transitions and once-per-second stream statistics. See
// VideoWorker.h's class-level comment for why m_frameMutex is kept
// separate from registryMutex; writing here on every decoded frame would
// reintroduce exactly the contention with the 50 Hz serial/control loop
// that split is meant to avoid. Mirrors ConnectionState's shape/intent for
// the serial link.
struct CameraState {
    bool        streaming       = false;
    int         width           = 0;
    int         height          = 0;
    double      advertisedFps   = 0.0;
    double      decodedFps      = 0.0;
    uint32_t    bitrateKbps     = 0;
    std::string codec;
    std::string profile;
    std::string transport;
};
