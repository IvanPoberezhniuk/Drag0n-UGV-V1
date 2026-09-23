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
#include "core/SafetyStateName.h"
#include <spdlog/spdlog.h>
#include "core/ChronoTypes.h"
#include <iterator>
#include <thread>

namespace {

void logDiagnosticLink(const char* name,
                       const CrsfFrameParser::DiagnosticLink& link) {
    const bool missing = link.telemetryAgeMs == 0xFFFFu;
    const bool unhealthy = missing || link.telemetryAgeMs > 500u ||
                           link.controlTxFailCount != 0u ||
                           link.uartCrcErrorCount != 0u ||
                           link.uartFormatErrorCount != 0u ||
                           link.faultMask != 0u;
    if (unhealthy) {
        if (missing) {
            spdlog::warn(
                "{}: TX={} fail={} RX=0 age=NONE state={} faults=0x{:02X} "
                "valid=0x{:02X} uart_crc={} format={} cmd_rx={} cmd_flags=0x{:02X} cmd_mask=0x{:02X}",
                name, link.controlTxCount, link.controlTxFailCount,
                safetyStateName(link.safetyState), link.faultMask,
                link.validMask, link.uartCrcErrorCount,
                link.uartFormatErrorCount, link.controlRxCount,
                link.lastControlFlags, link.lastEnabledMask);
        } else {
            spdlog::warn(
                "{}: TX={} fail={} RX={} age={}ms state={} faults=0x{:02X} "
                "valid=0x{:02X} uart_crc={} format={} cmd_rx={} cmd_flags=0x{:02X} cmd_mask=0x{:02X}",
                name, link.controlTxCount, link.controlTxFailCount,
                link.telemetryRxCount, link.telemetryAgeMs,
                safetyStateName(link.safetyState), link.faultMask,
                link.validMask, link.uartCrcErrorCount,
                link.uartFormatErrorCount, link.controlRxCount,
                link.lastControlFlags, link.lastEnabledMask);
        }
    } else {
        spdlog::info(
            "{}: TX={} fail=0 RX={} age={}ms state={} faults=0x00 "
            "valid=0x{:02X} uart_crc=0 format=0 cmd_rx={} cmd_flags=0x{:02X} cmd_mask=0x{:02X}",
            name, link.controlTxCount, link.telemetryRxCount,
            link.telemetryAgeMs, safetyStateName(link.safetyState),
            link.validMask, link.controlRxCount, link.lastControlFlags,
            link.lastEnabledMask);
    }
}

// Diagnostic-frame and BMS-frame staleness share this threshold: it's the
// point at which the underlying radio link (not just one message type)
// is considered gone. 6s, not the firmware's nominal 1000ms send period --
// bench logs show real inter-arrival gaps of 3-4s (CRSF frame contention
// with the RPM/diagnostic frames sharing the link), so a tight 3000ms
// threshold was firing every cycle and made the STM status icons flicker
// green/red/green on every log line.
constexpr long long kLinkLossTimeoutMs = 6000;

// Per-motor RPM telemetry goes stale faster than the link itself is
// declared lost -- this only blanks one field's display, not the STM
// online indicator.
constexpr long long kMotorRpmTimeoutMs = 1000;

bool isStale(Clock::time_point last, long long thresholdMs) {
    return last != Clock::time_point{} &&
           std::chrono::duration_cast<Ms>(Clock::now() - last).count() > thresholdMs;
}

} // namespace

SerialWorker::SerialWorker(AppState& state, const AppConfig& config)
    : m_state(state), m_config(config) {}

SerialWorker::~SerialWorker() { stop(); }

void SerialWorker::start() {
    m_running = true;
    m_thread = std::thread([this]{ loop(); });
}

void SerialWorker::requestStop() {
    m_running = false;
}

void SerialWorker::stop() {
    requestStop();
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

void SerialWorker::forceSafeState() {
    std::lock_guard<std::mutex> lk(m_state.registryMutex);
    auto& ctrl = m_state.registry.get<ControlState>(m_state.ugv);
    ctrl.armed = false;
    ctrl.estop = true;
    ctrl.throttle = 0.0f;
    ctrl.steering = 0.0f;
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
        // Nomad's USB UART is a secondary CRSF port. With no radio handset on
        // the module-bay pin, stock ELRS does not otherwise start its RF timer.
        // A one-shot bind command starts it; ELRS returns to the saved UID after
        // the short bind burst while neutral RC frames continue below.
        if (!m_rfStartSent) {
            const auto rfStart = buildTxBindCommandPacket();
            if (m_serial.write(rfStart.data(), rfStart.size())) {
                spdlog::info("SerialWorker: sent standalone USB RF-start command");
                m_rfStartSent = true;
            } else {
                spdlog::warn("SerialWorker: failed to send standalone USB RF-start command");
            }
        }

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
    forceSafeState();
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
            auto sinceLog = std::chrono::duration_cast<Ms>(
                Clock::now() - m_lastLinkStatsLog).count();
            if (m_lastLinkStatsLog == Clock::time_point{} || sinceLog > 1000) {
                spdlog::info("LinkStats: rssi1={} rssi2={} lq={}",
                             ls.rssi1, ls.rssi2, ls.lq);
                m_lastLinkStatsLog = Clock::now();
            }
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
        },
        [this](const CrsfFrameParser::RpmSensor& sensor) {
            std::lock_guard<std::mutex> lk(m_state.registryMutex);
            auto& t = m_state.registry.get<TelemetryState>(m_state.ugv);
            for (size_t i = 0; i < sensor.count; ++i) {
                const size_t motor = static_cast<size_t>(sensor.sourceId) + i;
                if (motor >= t.motorRpm.size()) break;
                t.motorRpm[motor] = static_cast<int>(sensor.rpm[i]);
                t.motorRpmValid[motor] = true;
                t.motorRpmLastReceived[motor] = Clock::now();
            }
            t.valid = true;
            t.lastReceived = Clock::now();
        },
        [this](const CrsfFrameParser::Diagnostic& diagnostic) {
            const bool rf = (diagnostic.flags & 0x01u) != 0u;
            const bool armed = (diagnostic.flags & 0x02u) != 0u;
            const bool estop = (diagnostic.flags & 0x04u) != 0u;
            spdlog::info(
                "UGV: RF={} ARM={} ESTOP={} mode={} throttle={:.1f}% "
                "steering={:.1f}% CRSF={} crc={}",
                rf ? "OK" : "LOST", armed ? "ON" : "OFF",
                estop ? "ON" : "OFF", diagnostic.driveMode,
                static_cast<double>(diagnostic.throttlePerMille) / 10.0,
                static_cast<double>(diagnostic.steeringPerMille) / 10.0,
                diagnostic.crsfChannelFrameCount,
                diagnostic.crsfCrcErrorCount);
            logDiagnosticLink("LEFT", diagnostic.left);
            logDiagnosticLink("RIGHT", diagnostic.right);

            auto isOnline = [](const CrsfFrameParser::DiagnosticLink& link) {
                return link.telemetryAgeMs != 0xFFFFu && link.telemetryAgeMs <= 1000u;
            };
            std::lock_guard<std::mutex> lk(m_state.registryMutex);
            auto& t = m_state.registry.get<TelemetryState>(m_state.ugv);
            t.stmLeftOnline  = isOnline(diagnostic.left);
            t.stmRightOnline = isOnline(diagnostic.right);
            t.stmLeftState      = diagnostic.left.safetyState;
            t.stmRightState     = diagnostic.right.safetyState;
            t.stmLeftFaultMask  = diagnostic.left.faultMask;
            t.stmRightFaultMask = diagnostic.right.faultMask;
            t.stmLeftAgeMs      = diagnostic.left.telemetryAgeMs;
            t.stmRightAgeMs     = diagnostic.right.telemetryAgeMs;
            t.stmLeftUptimeMs        = diagnostic.left.uptimeMs;
            t.stmRightUptimeMs       = diagnostic.right.uptimeMs;
            t.stmLeftStackFreeBytes  = diagnostic.left.stackFreeBytes;
            t.stmRightStackFreeBytes = diagnostic.right.stackFreeBytes;
            t.espUptimeMs      = diagnostic.espUptimeMs;
            t.espFreeHeapBytes = diagnostic.espFreeHeapBytes;
            t.diagnosticLastReceived = Clock::now();
        },
        [this](const CrsfFrameParser::BmsTelemetry& bms) {
            const bool connected = (bms.flags & 0x01u) != 0u;
            const bool valid = (bms.flags & 0x02u) != 0u;
            spdlog::info("BMS: connected={} valid={} soc={}% age={}ms",
                         connected, valid, bms.socPct, bms.frameAgeMs);
            std::lock_guard<std::mutex> lk(m_state.registryMutex);
            auto& t = m_state.registry.get<TelemetryState>(m_state.ugv);
            t.bmsConnected = connected;
            t.bmsValid = valid;
            t.batteryVoltage = bms.packVoltage;
            t.batteryCurrent = bms.packCurrent;
            t.batterySocPct = bms.socPct;
            t.batteryRemainingAh = bms.remainingCapacity;
            t.batteryFullAh = bms.fullCapacity;
            t.batteryCycleCount = bms.cycleCount;
            t.batteryCellMinMv = bms.cellMvMin;
            t.batteryCellMaxMv = bms.cellMvMax;
            t.batteryCellDeltaMv = bms.cellMvDelta;
            t.batteryCellMv = bms.cellMv;
            t.batteryCharging = bms.chargingEnabled;
            t.batteryDischarging = bms.dischargingEnabled;
            t.batteryChargerPlugged = bms.chargerPlugged;
            t.batteryBalancerStatus = bms.balancerStatus;
            t.batteryTempLowC = bms.tempLowC;
            t.batteryTempHighC = bms.tempHighC;
            t.batteryAlarmBits = bms.alarmBits;
            t.bmsLastReceived = Clock::now();
            t.valid = true;
            t.lastReceived = Clock::now();
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
                for (size_t motor = 0; motor < t.motorRpmValid.size(); ++motor) {
                    if (isStale(t.motorRpmLastReceived[motor], kMotorRpmTimeoutMs)) {
                        t.motorRpmValid[motor] = false;
                    }
                }
                // Diagnostic frame itself stopped arriving (radio link
                // dropped) -- telemetryAgeMs alone would otherwise stay
                // frozen at its last known value forever.
                if (isStale(t.diagnosticLastReceived, kLinkLossTimeoutMs)) {
                    t.stmLeftOnline  = false;
                    t.stmRightOnline = false;
                }
                if (isStale(t.bmsLastReceived, kLinkLossTimeoutMs)) {
                    t.bmsConnected = false;
                    t.bmsValid = false;
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
                forceSafeState();
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
