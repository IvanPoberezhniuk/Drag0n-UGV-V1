#pragma once
#include "core/AppState.h"
#include "config/AppConfig.h"
#include "io/SerialPort.h"
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

class SerialWorker {
public:
    SerialWorker(AppState& state, const AppConfig& config);
    ~SerialWorker();

    void start();
    void stop();

    void requestConnect(const std::string& port, uint32_t baudrate);
    void requestDisconnect();

private:
    struct Command {
        enum class Type { Connect, Disconnect } type;
        std::string port;
        uint32_t    baudrate = 0;
    };

    void loop();
    void drainCommands();
    void doConnect(const std::string& port, uint32_t baud);
    void doDisconnect();
    void readAndParse();
    void processRxBytes(const uint8_t* data, int len);
    void parseTelemetryFrame(uint8_t type, const uint8_t* payload, size_t len);

    AppState&        m_state;
    const AppConfig& m_config;
    SerialPort       m_serial;

    std::queue<Command> m_commands;
    std::mutex          m_commandMutex;

    std::thread       m_thread;
    std::atomic<bool> m_running{false};

    std::vector<uint8_t> m_rxBuf;

    std::string m_reconnectPort;
    uint32_t    m_reconnectBaud = 0;
    int         m_writeErrors   = 0;
    std::chrono::steady_clock::time_point m_nextReconnect{};
};
