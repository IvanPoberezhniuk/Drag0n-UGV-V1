#pragma once

struct InputFrame {
    float throttle     = 0.0f;
    float steering     = 0.0f;
    bool  hasAxes      = false;

    bool  arm          = false;
    bool  disarm       = false;
    bool  estop        = false;
    bool  toggleLights = false;
    int   setDriveMode = 0;      // 0 = no change, 1–3 = set
};

class IInputSource {
public:
    virtual ~IInputSource() = default;
    virtual InputFrame  poll()        = 0;
    virtual bool        isConnected() const = 0;
    virtual const char* name()        const = 0;
};
