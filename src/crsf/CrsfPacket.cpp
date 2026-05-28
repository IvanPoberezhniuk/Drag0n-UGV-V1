#include "crsf/CrsfPacket.h"
#include <cstring>

// CRC8 DVB-S2, polynomial 0xD5
static uint8_t crc8_dvbs2(const uint8_t* data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0xD5;
            else
                crc <<= 1;
        }
    }
    return crc;
}

// Pack 16 channels (11-bit each, LSB-first) into 22 bytes.
// Bit layout: channel 0 occupies bits [0..10], channel 1 bits [11..21], etc.
static void packChannels(const RcChannels& ch, uint8_t* out) {
    memset(out, 0, 22);
    int bitOffset = 0;
    for (int i = 0; i < 16; ++i) {
        uint32_t val = ch.ch[i] & 0x7FF;  // clamp to 11 bits
        int byteIdx = bitOffset / 8;
        int bitIdx  = bitOffset % 8;
        // Write up to 3 bytes to cover the 11-bit span
        out[byteIdx]     |= static_cast<uint8_t>(val << bitIdx);
        out[byteIdx + 1] |= static_cast<uint8_t>(val >> (8 - bitIdx));
        if (bitIdx > 5)
            out[byteIdx + 2] |= static_cast<uint8_t>(val >> (16 - bitIdx));
        bitOffset += 11;
    }
}

// Wire format: [0xC8][length=0x18][0x16][22 bytes payload][CRC] = 26 bytes
// length field = frame_type(1) + payload(22) + CRC(1) = 24 = 0x18
std::array<uint8_t, 26> buildRcChannelsPacket(const RcChannels& channels) {
    std::array<uint8_t, 26> pkt{};
    pkt[0] = CRSF_SYNC;
    pkt[1] = 0x18;               // length: 24 bytes (type + payload + crc)
    pkt[2] = CRSF_FRAMETYPE_RC;
    packChannels(channels, &pkt[3]);
    // CRC covers frame_type + payload (bytes 2..24, i.e. 23 bytes)
    pkt[25] = crc8_dvbs2(&pkt[2], 23);
    return pkt;
}
