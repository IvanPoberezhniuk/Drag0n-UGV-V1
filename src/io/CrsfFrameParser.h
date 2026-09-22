#pragma once
#include "crsf/CrsfTypes.h"
#include "crsf/CrsfPacket.h"
#include <vector>
#include <array>
#include <cstdint>
#include <functional>
#include <algorithm>
#include <spdlog/spdlog.h>

class CrsfFrameParser {
public:
    struct LinkStats    { uint8_t rssi1, rssi2, lq; };
    struct BatterySensor{ float voltage; };
    struct RpmSensor {
        uint8_t sourceId = 0;
        std::array<int32_t, 6> rpm{};
        size_t count = 0;
    };
    struct DiagnosticLink {
        uint32_t controlTxCount = 0;
        uint16_t controlTxFailCount = 0;
        uint32_t telemetryRxCount = 0;
        uint16_t telemetryAgeMs = 0xFFFFu;
        uint16_t uartCrcErrorCount = 0;
        uint16_t uartFormatErrorCount = 0;
        uint8_t safetyState = 0;
        uint8_t faultMask = 0;
        uint8_t validMask = 0;
        uint8_t controlRxCount = 0;
        uint8_t lastControlFlags = 0;
        uint8_t lastEnabledMask = 0;
        uint32_t uptimeMs = 0;
        uint16_t stackFreeBytes = 0;
    };
    struct Diagnostic {
        uint8_t flags = 0;
        uint8_t driveMode = 0;
        int16_t throttlePerMille = 0;
        int16_t steeringPerMille = 0;
        uint32_t crsfChannelFrameCount = 0;
        uint16_t crsfCrcErrorCount = 0;
        DiagnosticLink left;
        DiagnosticLink right;
        uint32_t espUptimeMs = 0;
        uint32_t espFreeHeapBytes = 0;
    };
    struct BmsTelemetry {
        uint8_t flags = 0;
        uint8_t socPct = 0;
        uint16_t frameAgeMs = 0xFFFFu;
        float packVoltage = 0.0f;
        float packCurrent = 0.0f;
        float remainingCapacity = 0.0f;
        float fullCapacity = 0.0f;
        uint16_t cycleCount = 0;
        uint16_t cellMvMin = 0;
        uint16_t cellMvMax = 0;
        uint16_t cellMvDelta = 0;
        int8_t tempLowC = 0;
        int8_t tempHighC = 0;
        uint32_t alarmBits = 0;
        std::array<uint16_t, 4> cellMv{}; // pack is confirmed 4S
        bool chargingEnabled = false;
        bool dischargingEnabled = false;
        bool chargerPlugged = false;
        uint8_t balancerStatus = 0; // 0 off, 1 charging balancer, 2 discharging balancer
    };

    using OnLinkStats = std::function<void(const LinkStats&)>;
    using OnBattery   = std::function<void(const BatterySensor&)>;
    using OnRpm       = std::function<void(const RpmSensor&)>;
    using OnDiagnostic = std::function<void(const Diagnostic&)>;
    using OnBms = std::function<void(const BmsTelemetry&)>;

    static constexpr size_t kMaxBufSize = 512;

    void feed(const uint8_t* data, int len,
              const OnLinkStats& onLink, const OnBattery& onBattery,
              const OnRpm& onRpm, const OnDiagnostic& onDiagnostic,
              const OnBms& onBms) {
        m_buf.insert(m_buf.end(), data, data + len);

        while (m_buf.size() >= 4) {
            if (!isAddressByte(m_buf[0])) {
                m_buf.erase(m_buf.begin());
                continue;
            }
            uint8_t frameLen = m_buf[1];
            if (frameLen < 2) { m_buf.erase(m_buf.begin()); continue; }

            size_t total = 2u + frameLen;
            if (m_buf.size() < total) break;

            uint8_t expected = crc8_dvbs2(m_buf.data() + 2, frameLen - 1);
            if (expected != m_buf[total - 1]) {
                spdlog::debug("CRSF: CRC mismatch, discarding byte");
                m_buf.erase(m_buf.begin());
                continue;
            }

            uint8_t        type       = m_buf[2];
            const uint8_t* payload    = m_buf.data() + 3;
            size_t         payloadLen = frameLen - 2;
            dispatch(type, payload, payloadLen, onLink, onBattery, onRpm,
                     onDiagnostic, onBms);
            m_buf.erase(m_buf.begin(), m_buf.begin() + total);
        }

        if (m_buf.size() > kMaxBufSize)
            m_buf.clear();
    }

    void clear() { m_buf.clear(); }

private:
    static uint16_t getU16Le(const uint8_t* value) {
        return static_cast<uint16_t>(value[0]) |
               (static_cast<uint16_t>(value[1]) << 8u);
    }

    static int16_t getI16Le(const uint8_t* value) {
        return static_cast<int16_t>(getU16Le(value));
    }

    static uint32_t getU32Le(const uint8_t* value) {
        return static_cast<uint32_t>(value[0]) |
               (static_cast<uint32_t>(value[1]) << 8u) |
               (static_cast<uint32_t>(value[2]) << 16u) |
               (static_cast<uint32_t>(value[3]) << 24u);
    }

    static int32_t getI32Le(const uint8_t* value) {
        return static_cast<int32_t>(getU32Le(value));
    }

    static DiagnosticLink decodeDiagnosticLink(const uint8_t* payload) {
        DiagnosticLink link;
        link.controlTxCount = getU32Le(&payload[0]);
        link.controlTxFailCount = getU16Le(&payload[4]);
        link.telemetryRxCount = getU32Le(&payload[6]);
        link.telemetryAgeMs = getU16Le(&payload[10]);
        link.uartCrcErrorCount = getU16Le(&payload[12]);
        link.uartFormatErrorCount = getU16Le(&payload[14]);
        link.safetyState = payload[16];
        link.faultMask = payload[17];
        link.validMask = payload[18];
        link.controlRxCount = payload[19];
        link.lastControlFlags = payload[20];
        link.lastEnabledMask = payload[21];
        link.uptimeMs = getU32Le(&payload[22]);
        link.stackFreeBytes = getU16Le(&payload[26]);
        return link;
    }

    static bool isAddressByte(uint8_t value) {
        // CRSF allows the serial sync byte or a routed device address here.
        return value == CRSF_SYNC || value == 0x00u || value == 0xEAu ||
               value == 0xECu || value == 0xEEu;
    }

    void dispatch(uint8_t type, const uint8_t* payload, size_t len,
                  const OnLinkStats& onLink, const OnBattery& onBattery,
                  const OnRpm& onRpm, const OnDiagnostic& onDiagnostic,
                  const OnBms& onBms) {
        if (type == CRSF_FRAMETYPE_LINK_STATISTICS && len >= 10) {
            onLink({ payload[0], payload[1], payload[2] });
        } else if (type == CRSF_FRAMETYPE_BATTERY_SENSOR && len >= 8) {
            float v = static_cast<float>((payload[0] << 8) | payload[1]) * 0.1f;
            onBattery({ v });
        } else if (type == CRSF_FRAMETYPE_RPM_SENSOR && len >= 4 &&
                   ((len - 1u) % 3u) == 0u) {
            RpmSensor sensor;
            sensor.sourceId = payload[0];
            sensor.count = std::min<size_t>((len - 1u) / 3u, sensor.rpm.size());
            for (size_t i = 0; i < sensor.count; ++i) {
                const size_t offset = 1u + i * 3u;
                uint32_t raw = (static_cast<uint32_t>(payload[offset]) << 16u) |
                               (static_cast<uint32_t>(payload[offset + 1u]) << 8u) |
                               static_cast<uint32_t>(payload[offset + 2u]);
                if ((raw & 0x00800000u) != 0u)
                    raw |= 0xFF000000u;
                sensor.rpm[i] = static_cast<int32_t>(raw);
            }
            onRpm(sensor);
        } else if (type == CRSF_FRAMETYPE_UGV_DIAGNOSTIC && len >= 80u &&
                   payload[0] == 'U' && payload[1] == 'G' &&
                   payload[2] == 'V' && payload[3] == 3u) {
            Diagnostic diagnostic;
            diagnostic.flags = payload[4];
            diagnostic.driveMode = payload[5];
            diagnostic.throttlePerMille = getI16Le(&payload[6]);
            diagnostic.steeringPerMille = getI16Le(&payload[8]);
            diagnostic.crsfChannelFrameCount = getU32Le(&payload[10]);
            diagnostic.crsfCrcErrorCount = getU16Le(&payload[14]);
            diagnostic.left = decodeDiagnosticLink(&payload[16]);
            diagnostic.right = decodeDiagnosticLink(&payload[44]);
            diagnostic.espUptimeMs = getU32Le(&payload[72]);
            diagnostic.espFreeHeapBytes = getU32Le(&payload[76]);
            onDiagnostic(diagnostic);
        } else if (type == CRSF_FRAMETYPE_UGV_BMS && len >= 47u &&
                   payload[0] == 'B' && payload[1] == 'M' &&
                   payload[2] == 'S' && payload[3] == 2u) {
            BmsTelemetry bms;
            bms.flags = payload[4];
            bms.socPct = payload[5];
            bms.frameAgeMs = getU16Le(&payload[6]);
            bms.packVoltage = static_cast<float>(getU32Le(&payload[8])) * 0.001f;
            bms.packCurrent = static_cast<float>(getI32Le(&payload[12])) * 0.001f;
            bms.remainingCapacity = static_cast<float>(getU32Le(&payload[16])) * 0.001f;
            bms.fullCapacity = static_cast<float>(getU32Le(&payload[20])) * 0.001f;
            bms.cycleCount = getU16Le(&payload[24]);
            bms.cellMvMin = getU16Le(&payload[26]);
            bms.cellMvMax = getU16Le(&payload[28]);
            bms.cellMvDelta = getU16Le(&payload[30]);
            bms.tempLowC = static_cast<int8_t>(payload[32]);
            bms.tempHighC = static_cast<int8_t>(payload[33]);
            bms.alarmBits = getU32Le(&payload[34]);
            for (size_t i = 0; i < bms.cellMv.size(); ++i) {
                bms.cellMv[i] = getU16Le(&payload[38 + i * 2]);
            }
            bms.chargingEnabled    = (payload[46] & 0x01u) != 0u;
            bms.dischargingEnabled = (payload[46] & 0x02u) != 0u;
            bms.chargerPlugged     = (payload[46] & 0x04u) != 0u;
            bms.balancerStatus     = (payload[46] >> 3) & 0x03u;
            onBms(bms);
        }
    }

    std::vector<uint8_t> m_buf;
};
