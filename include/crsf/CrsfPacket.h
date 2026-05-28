#pragma once
#include "CrsfTypes.h"
#include <array>
#include <cstdint>

std::array<uint8_t, 26> buildRcChannelsPacket(const RcChannels& channels);
