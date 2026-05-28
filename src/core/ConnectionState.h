#pragma once
#include <string>
#include <vector>

enum class ConnectionStatus {
    Disconnected,
    Connecting,
    Connected,
    Error
};

struct ConnectionState {
    ConnectionStatus status      = ConnectionStatus::Disconnected;
    std::string      portName;
    uint32_t         baudrate    = 420000;
    uint32_t         pktPerSec  = 0;
    std::string      errorMessage;
    std::vector<std::string> availablePorts;
};
