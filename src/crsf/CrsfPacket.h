#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include "crsf/CrsfTypes.h"

uint8_t crc8_dvbs2(const uint8_t* data, size_t len);
std::array<uint8_t, 26> buildRcChannelsPacket(const RcChannels& channels);
