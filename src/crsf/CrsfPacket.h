#pragma once
#include <array>
#include "crsf/CrsfTypes.h"

// Build a 26-byte CRSF RC_CHANNELS_PACKED frame ready to send over serial.
std::array<uint8_t, 26> buildRcChannelsPacket(const RcChannels& channels);
