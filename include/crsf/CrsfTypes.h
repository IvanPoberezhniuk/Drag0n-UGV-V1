#pragma once
#include <cstdint>

constexpr uint8_t  CRSF_SYNC         = 0xC8;
constexpr uint8_t  CRSF_FRAMETYPE_RC = 0x16;
constexpr uint16_t CH_MIN            = 172;
constexpr uint16_t CH_CENTER         = 992;
constexpr uint16_t CH_MAX            = 1811;

struct RcChannels {
    uint16_t ch[16];
};
