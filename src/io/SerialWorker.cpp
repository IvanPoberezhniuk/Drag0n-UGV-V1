#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <timeapi.h>
#include "io/SerialWorker.h"
#include "core/ControlState.h"
#include "core/SafetyState.h"
#include "core/ConnectionState.h"
#include "core/TelemetryState.h"
#include "services/SafetyService.h"
#include "services/ControlService.h"
#include "crsf/CrsfPacket.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>

using Clock = std::chrono::steady_clock;
using Ms    = std::chrono::milliseconds;
using Us    = std::chrono::microseconds;

SerialWorker::SerialWorker(AppState& state, const AppConfig& config)
    : m_state(state), m_config(config) {}

SerialWorker::~SerialWorker() { stop(); }

void SerialWorker::start() {
    m_running = true;
    m_thread = std::thread([this]{ loop(); });
}

void SerialWorker::stop() {
    m_running = false;
    if (m_thread.joinable()) m_thread.join();
}

void SerialWorker::requestConnect(const std::string& port, uint32_t baudrate) {
    std::lock_guard<std::mutex> lk(m_commandMutex);
    m_commands.push({Command::Type::Connect, port, baudrate});
}

void SerialWorker::requestDisconnect() {
    std::lock_guard<std::mutex> lk(m_commandMutex);
    m_commands.push({Command::Type::Disconnect, {}, 0});
}

void SerialWorker::doConnect(const std::string& port, uint32_t baud) {
    if (m_serial.isOpen()) m_serial.close();

    {
        std::lock_guard<std::mutex> lk(m_state.registryMutex);
        auto& conn = m_state.registry.get<ConnectionState>(m_state.ugv);
        conn.status    = ConnectionStatus::Connecting;
        conn.portName  = port;
        conn.errorMessage.clear();
    }

    std::string resolvedPort = port;
    if (resolvedPort == "auto") {
        resolvedPort = SerialPort::autoDetect();
        if (resolvedPort.empty()) {
            std::lock_guard<std::mutex> lk(m_state.registryMutex);
            auto& conn = m_state.registry.get<ConnectionState>(m_state.ugv);
            conn.status       = ConnectionStatus::Error;
            conn.errorMessage = "Auto-detect: no ELRS TX module found";
            return;
        }
    }

    if (m_serial.open(resolvedPort, baud)) {
        std::lock_guard<std::mutex> lk(m_state.registryMutex);
        auto& conn = m_state.registry.get<ConnectionState>(m_state.ugv);
        conn.status   = ConnectionStatus::Connected;
        conn.portName = resolvedPort;
    } else {
        std::lock_guard<std::mutex> lk(m_state.registryMutex);
        auto& conn = m_state.registry.get<ConnectionState>(m_state.ugv);
        conn.status       = ConnectionStatus::Error;
        conn.errorMessage = "Failed to open " + resolvedPort;
    }
}

void SerialWorker::doDisconnect() {
    m_serial.close();
    std::lock_guard<std::mutex> lk(m_state.registryMutex);
    auto& conn = m_state.registry.get<ConnectionState>(m_state.ugv);
    conn.status = ConnectionStatus::Disconnected;
    conn.pktPerSec = 0;
}

void SerialWorker::drainCommands() {
    std::queue<Command> local;
    {
        std::lock_guard<std::mutex> lk(m_commandMutex);
        std::swap(local, m_commands);
    }
    while (!local.empty()) {
        auto& cmd = local.front();
        if (cmd.type == Command::Type::Connect)
            doConnect(cmd.port, cmd.baudrate);
        else
            doDisconnect();
        local.pop();
    }
}

void SerialWorker::loop() {
    timeBeginPeriod(1);

    const uint32_t rateHz = m_config.control.rateHz > 0 ? m_config.control.rateHz : 50;
    const auto intervalUs = Us(1'000'000 / rateHz);

    uint64_t frameCount = 0;
    auto statsTimer = Clock::now();

    while (m_running) {
        auto frameStart = Clock::now();

        drainCommands();

        // Snapshot control + safety state
        ControlState ctrl;
        SafetyState  safety;
        {
            std::lock_guard<std::mutex> lk(m_state.registryMutex);
            ctrl   = m_state.registry.get<ControlState>(m_state.ugv);
            safety = m_state.registry.get<SafetyState>(m_state.ugv);
        }

        // Apply safety rules
        SafetyService::apply(ctrl, safety, m_config.control.failsafeTimeoutMs);

        // Write safety state back
        {
            std::lock_guard<std::mutex> lk(m_state.registryMutex);
            m_state.registry.get<SafetyState>(m_state.ugv) = safety;
        }

        // Build and send CRSF packet
        if (m_serial.isOpen()) {
            auto rc  = DroneControlService::mapChannels(ctrl, m_config.channels);
            auto pkt = buildRcChannelsPacket(rc);
            m_serial.write(pkt.data(), pkt.size());
            ++frameCount;
        }

        // Update pkt/s stats once per second
        auto elapsed = std::chrono::duration_cast<Ms>(Clock::now() - statsTimer).count();
        if (elapsed >= 1000) {
            std::lock_guard<std::mutex> lk(m_state.registryMutex);
            m_state.registry.get<ConnectionState>(m_state.ugv).pktPerSec =
                static_cast<uint32_t>(frameCount);
            frameCount = 0;
            statsTimer = Clock::now();
        }

        // Sleep for remainder of frame interval
        auto frameDuration = Clock::now() - frameStart;
        if (frameDuration < intervalUs)
            std::this_thread::sleep_for(intervalUs - frameDuration);
    }

    // Send one failsafe packet then close
    if (m_serial.isOpen()) {
        RcChannels safe{};
        for (auto& c : safe.ch) c = CH_CENTER;
        safe.ch[m_config.channels.arm   - 1] = CH_MIN;
        safe.ch[m_config.channels.estop - 1] = CH_MAX;
        auto pkt = buildRcChannelsPacket(safe);
        m_serial.write(pkt.data(), pkt.size());
        m_serial.close();
    }

    timeEndPeriod(1);
}
