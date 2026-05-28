#pragma once
#include <cstdint>

constexpr uint8_t  CRSF_SYNC                      = 0xC8;
constexpr uint8_t  CRSF_FRAMETYPE_RC              = 0x16;
constexpr uint8_t  CRSF_FRAMETYPE_BATTERY_SENSOR  = 0x08;
constexpr uint8_t  CRSF_FRAMETYPE_LINK_STATISTICS = 0x14;
constexpr uint16_t CH_MIN            = 172;
constexpr uint16_t CH_CENTER         = 992;
constexpr uint16_t CH_MAX            = 1811;

struct RcChannels {
    uint16_t ch[16];
};
