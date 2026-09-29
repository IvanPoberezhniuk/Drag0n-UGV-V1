#pragma once

struct SafetyState {
    bool failsafeActive  = false;
    bool estopLatched    = false;
    bool connectionLost  = true;   // nothing is connected at startup
};
