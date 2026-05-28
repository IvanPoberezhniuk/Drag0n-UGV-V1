#pragma once
#include "core/AppState.h"
#include "config/AppConfig.h"
#include "io/SerialPort.h"
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <string>

class SerialWorker {
public:
    SerialWorker(AppState& state, const AppConfig& config);
    ~SerialWorker();

    void start();
    void stop();

    // Push a connect command; safe to call from main thread.
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

    AppState&        m_state;
    const AppConfig& m_config;
    SerialPort       m_serial;

    std::queue<Command> m_commands;
    std::mutex          m_commandMutex;

    std::thread       m_thread;
    std::atomic<bool> m_running{false};
};
