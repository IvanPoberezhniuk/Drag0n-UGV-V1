#pragma once
#include <cstdint>

constexpr uint8_t  CRSF_SYNC                      = 0xC8;
constexpr uint8_t  CRSF_FRAMETYPE_RC              = 0x16;
constexpr uint8_t  CRSF_FRAMETYPE_BATTERY_SENSOR  = 0x08;
constexpr uint8_t  CRSF_FRAMETYPE_RPM_SENSOR      = 0x0C;
constexpr uint8_t  CRSF_FRAMETYPE_LINK_STATISTICS = 0x14;
constexpr uint8_t  CRSF_FRAMETYPE_UGV_DIAGNOSTIC  = 0x80;
constexpr uint8_t  CRSF_FRAMETYPE_UGV_BMS         = 0x81;

// Device-parameter protocol (what an ExpressLRS Lua script speaks). These are
// "extended" frames: [dest][origin] follow the type byte.
constexpr uint8_t  CRSF_FRAMETYPE_DEVICE_PING     = 0x28;
constexpr uint8_t  CRSF_FRAMETYPE_DEVICE_INFO     = 0x29;
constexpr uint8_t  CRSF_FRAMETYPE_PARAM_ENTRY     = 0x2B;
constexpr uint8_t  CRSF_FRAMETYPE_PARAM_READ      = 0x2C;
constexpr uint8_t  CRSF_FRAMETYPE_PARAM_WRITE     = 0x2D;
constexpr uint8_t  CRSF_ADDRESS_BROADCAST         = 0x00;
constexpr uint8_t  CRSF_ADDRESS_HANDSET           = 0xEA; // what this PC app presents itself as
constexpr uint8_t  CRSF_ADDRESS_RECEIVER          = 0xEC;
constexpr uint8_t  CRSF_ADDRESS_TX_MODULE         = 0xEE;
constexpr uint16_t CH_MIN            = 172;
constexpr uint16_t CH_CENTER         = 992;
constexpr uint16_t CH_MAX            = 1811;

struct RcChannels {
    uint16_t ch[16];
};
