#pragma once
#include "core/AppState.h"
#include "config/AppConfig.h"
#include "io/SerialPort.h"
#include "io/CrsfFrameParser.h"
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <string>
#include <chrono>
#include <cstdint>

class SerialWorker {
public:
    SerialWorker(AppState& state, const AppConfig& config);
    ~SerialWorker();

    void start();
    void stop();

    // Signals the loop to unwind without joining -- see
    // MainWindow::closeEvent, which requestStop()s every worker before
    // join()ing any of them so the total shutdown wait is bounded by the
    // slowest worker instead of their sum.
    void requestStop();

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
    void forceSafeState();

    AppState&        m_state;
    const AppConfig& m_config;
    SerialPort       m_serial;
    CrsfFrameParser  m_parser;

    std::queue<Command> m_commands;
    std::mutex          m_commandMutex;

    std::thread       m_thread;
    std::atomic<bool> m_running{false};

    std::string m_reconnectPort;
    uint32_t    m_reconnectBaud = 0;
    int         m_writeErrors   = 0;
    std::chrono::steady_clock::time_point m_nextReconnect{};
};
