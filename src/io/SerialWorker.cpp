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
#include "core/ChronoTypes.h"
#include <thread>

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
            m_nextReconnect   = Clock::now() + Ms(m_config.serial.reconnectDelayMs);
            return;
        }
    }

    if (m_serial.open(resolvedPort, baud)) {
        std::lock_guard<std::mutex> lk(m_state.registryMutex);
        auto& conn = m_state.registry.get<ConnectionState>(m_state.ugv);
        conn.status   = ConnectionStatus::Connected;
        conn.portName = resolvedPort;
        m_reconnectPort = port;
        m_reconnectBaud = baud;
        m_writeErrors   = 0;
        m_parser.clear();
    } else {
        std::lock_guard<std::mutex> lk(m_state.registryMutex);
        auto& conn = m_state.registry.get<ConnectionState>(m_state.ugv);
        conn.status       = ConnectionStatus::Error;
        conn.errorMessage = "Failed to open " + resolvedPort;
        m_nextReconnect   = Clock::now() + Ms(m_config.serial.reconnectDelayMs);
    }
}

void SerialWorker::doDisconnect() {
    m_serial.close();
    m_reconnectPort.clear();
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

void SerialWorker::readAndParse() {
    uint8_t tmp[256];
    int n = m_serial.read(tmp, sizeof(tmp));
    if (n <= 0) return;

    m_parser.feed(tmp, n,
        [this](const CrsfFrameParser::LinkStats& ls) {
            std::lock_guard<std::mutex> lk(m_state.registryMutex);
            auto& t    = m_state.registry.get<TelemetryState>(m_state.ugv);
            t.rssi1    = ls.rssi1;
            t.rssi2    = ls.rssi2;
            t.lq       = ls.lq;
            t.valid    = true;
            t.lastReceived = Clock::now();
        },
        [this](const CrsfFrameParser::BatterySensor& bat) {
            std::lock_guard<std::mutex> lk(m_state.registryMutex);
            auto& t          = m_state.registry.get<TelemetryState>(m_state.ugv);
            t.batteryVoltage = bat.voltage;
            t.valid          = true;
            t.lastReceived   = Clock::now();
        }
    );
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

        ControlState ctrl;
        SafetyState  safety;
        {
            std::lock_guard<std::mutex> lk(m_state.registryMutex);
            ctrl   = m_state.registry.get<ControlState>(m_state.ugv);
            safety = m_state.registry.get<SafetyState>(m_state.ugv);
        }

        SafetyService::apply(ctrl, safety, m_config.control.failsafeTimeoutMs);

        {
            std::lock_guard<std::mutex> lk(m_state.registryMutex);
            m_state.registry.get<SafetyState>(m_state.ugv) = safety;
        }

        if (m_serial.isOpen()) {
            readAndParse();

            {
                std::lock_guard<std::mutex> lk(m_state.registryMutex);
                auto& t = m_state.registry.get<TelemetryState>(m_state.ugv);
                if (t.valid) {
                    auto msSince = std::chrono::duration_cast<Ms>(
                        Clock::now() - t.lastReceived).count();
                    if (msSince > static_cast<long long>(m_config.control.telemetryTimeoutMs)) {
                        t.valid = false;
                        spdlog::warn("SerialWorker: telemetry timeout ({}ms)", msSince);
                    }
                }
            }

            auto rc  = ChannelMapper::mapChannels(ctrl, m_config.channels);
            auto pkt = buildRcChannelsPacket(rc);
            if (m_serial.write(pkt.data(), pkt.size())) {
                ++frameCount;
                m_writeErrors = 0;
            } else if (++m_writeErrors >= static_cast<int>(m_config.control.writeErrorThreshold)) {
                spdlog::warn("SerialWorker: {} write errors — closing for reconnect", m_writeErrors);
                m_serial.close();
                m_writeErrors   = 0;
                m_nextReconnect = Clock::now() + Ms(m_config.serial.reconnectDelayMs);
                std::lock_guard<std::mutex> lk(m_state.registryMutex);
                auto& conn = m_state.registry.get<ConnectionState>(m_state.ugv);
                conn.status       = ConnectionStatus::Error;
                conn.errorMessage = "Connection lost — retrying";
                conn.pktPerSec    = 0;
            }
        } else if (!m_reconnectPort.empty() && Clock::now() >= m_nextReconnect) {
            m_nextReconnect = Clock::now() + Ms(m_config.serial.reconnectDelayMs);
            spdlog::info("SerialWorker: auto-reconnecting to {}", m_reconnectPort);
            doConnect(m_reconnectPort, m_reconnectBaud);
        }

        auto elapsed = std::chrono::duration_cast<Ms>(Clock::now() - statsTimer).count();
        if (elapsed >= 1000) {
            std::lock_guard<std::mutex> lk(m_state.registryMutex);
            m_state.registry.get<ConnectionState>(m_state.ugv).pktPerSec =
                static_cast<uint32_t>(frameCount);
            frameCount = 0;
            statsTimer = Clock::now();
        }

        auto frameDuration = Clock::now() - frameStart;
        if (frameDuration < intervalUs)
            std::this_thread::sleep_for(intervalUs - frameDuration);
    }

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
