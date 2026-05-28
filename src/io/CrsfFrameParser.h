#pragma once
#include "crsf/CrsfTypes.h"
#include "crsf/CrsfPacket.h"
#include <vector>
#include <cstdint>
#include <functional>
#include <spdlog/spdlog.h>

class CrsfFrameParser {
public:
    struct LinkStats    { uint8_t rssi1, rssi2, lq; };
    struct BatterySensor{ float voltage; };

    using OnLinkStats = std::function<void(const LinkStats&)>;
    using OnBattery   = std::function<void(const BatterySensor&)>;

    static constexpr size_t kMaxBufSize = 512;

    void feed(const uint8_t* data, int len,
              const OnLinkStats& onLink, const OnBattery& onBattery) {
        m_buf.insert(m_buf.end(), data, data + len);

        while (m_buf.size() >= 4) {
            if (m_buf[0] != CRSF_SYNC) {
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
            dispatch(type, payload, payloadLen, onLink, onBattery);
            m_buf.erase(m_buf.begin(), m_buf.begin() + total);
        }

        if (m_buf.size() > kMaxBufSize)
            m_buf.clear();
    }

    void clear() { m_buf.clear(); }

private:
    void dispatch(uint8_t type, const uint8_t* payload, size_t len,
                  const OnLinkStats& onLink, const OnBattery& onBattery) {
        if (type == CRSF_FRAMETYPE_LINK_STATISTICS && len >= 10) {
            onLink({ payload[0], payload[1], payload[2] });
        } else if (type == CRSF_FRAMETYPE_BATTERY_SENSOR && len >= 8) {
            float v = static_cast<float>((payload[0] << 8) | payload[1]) * 0.1f;
            onBattery({ v });
        }
    }

    std::vector<uint8_t> m_buf;
};
