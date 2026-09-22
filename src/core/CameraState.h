#pragma once

// Camera/video-feed connection status, published by VideoWorker into
// AppState's component registry (under AppState::registryMutex) on
// connect/disconnect transitions only -- never per-frame. See
// VideoWorker.h's class-level comment for why m_frameMutex is kept
// separate from registryMutex; writing here on every decoded frame would
// reintroduce exactly the contention with the 50 Hz serial/control loop
// that split is meant to avoid. Mirrors ConnectionState's shape/intent for
// the serial link.
struct CameraState {
    bool streaming = false;
};
