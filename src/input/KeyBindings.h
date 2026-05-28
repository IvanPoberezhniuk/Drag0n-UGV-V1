#pragma once
#include <array>

struct ActionBinding {
    const char* name;
    int         key1;
    int         key2;
};

struct KeyBindings {
    enum Action {
        ThrottleForward  = 0,
        ThrottleBackward = 1,
        SteerLeft        = 2,
        SteerRight       = 3,
        ArmDisarm        = 4,
        EStop            = 5,
        ToggleLights     = 6,
        DriveMode1       = 7,
        DriveMode2       = 8,
        DriveMode3       = 9,
        Count            = 10
    };

    // Qt::Key values: letters/digits match their ASCII / VK codes directly.
    // Qt::Key_Return = 0x01000004, Qt::Key_Space = 0x20
    std::array<ActionBinding, Count> actions = {{
        { "Throttle Forward",  'W',        0 },
        { "Throttle Backward", 'S',        0 },
        { "Steer Left",        'A',        0 },
        { "Steer Right",       'D',        0 },
        { "Arm / Disarm",      0x01000004, 0 },
        { "E-Stop",            0x20,       0 },
        { "Toggle Lights",     'T',        0 },
        { "Drive Mode 2WD",    '1',        0 },
        { "Drive Mode 4WD",    '2',        0 },
        { "Drive Mode 6WD",    '3',        0 },
    }};
};
