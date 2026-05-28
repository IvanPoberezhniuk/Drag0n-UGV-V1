#pragma once
#include <string>
#include <cstdint>

struct ConnectEvent    { std::string port; uint32_t baudrate; };
struct DisconnectEvent {};
struct ArmEvent        { bool armed; };
struct EstopEvent      {};
struct DriveInputEvent { float throttle; float steering; int mode; bool lightsOn; };
