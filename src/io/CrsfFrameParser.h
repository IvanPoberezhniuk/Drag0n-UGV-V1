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

    using OnLinkStats = std::function<void(const LinkStats&)>;
    using OnBattery   = std::function<void(const BatterySensor&)>;
    using OnRpm       = std::function<void(const RpmSensor&)>;

    static constexpr size_t kMaxBufSize = 512;

    void feed(const uint8_t* data, int len,
              const OnLinkStats& onLink, const OnBattery& onBattery,
              const OnRpm& onRpm) {
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
            dispatch(type, payload, payloadLen, onLink, onBattery, onRpm);
            m_buf.erase(m_buf.begin(), m_buf.begin() + total);
        }

        if (m_buf.size() > kMaxBufSize)
            m_buf.clear();
    }

    void clear() { m_buf.clear(); }

private:
    static bool isAddressByte(uint8_t value) {
        // CRSF allows the serial sync byte or a routed device address here.
        return value == CRSF_SYNC || value == 0x00u || value == 0xEAu ||
               value == 0xECu || value == 0xEEu;
    }

    void dispatch(uint8_t type, const uint8_t* payload, size_t len,
                  const OnLinkStats& onLink, const OnBattery& onBattery,
                  const OnRpm& onRpm) {
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
        }
    }

    std::vector<uint8_t> m_buf;
};
