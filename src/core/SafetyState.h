#pragma once

struct SafetyState {
    bool failsafeActive  = false;
    bool estopLatched    = false;
    bool connectionLost  = false;
};
