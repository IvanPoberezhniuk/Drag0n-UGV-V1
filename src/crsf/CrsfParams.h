#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// CRSF device-parameter wire codec: the ping / device-info / parameter-entry /
// parameter-read / parameter-write frames an ExpressLRS Lua script uses to
// browse and edit a TX module's or receiver's settings.
//
// Pure encode/decode -- no Qt, no threads, no I/O -- so it can be unit tested
// (tests/crsf_params_test.cpp). The request/response state machine lives in
// io/RadioParamClient.
//
// Wire-format notes (from the ExpressLRS Lua protocol; not yet confirmed
// against this project's Nomad/XR4 firmware -- see the bench probe in the plan):
//   * multi-byte numeric field values are big-endian;
//   * an entry (0x2B) may be split over several chunks, each carrying
//     [field id][chunks remaining][chunk bytes]; the chunk bytes concatenate
//     to [parent][type|hidden<<7][name\0][type-specific data].
namespace crsf {

enum class FieldType : uint8_t {
    Uint8         = 0,
    Int8          = 1,
    Uint16        = 2,
    Int16         = 3,
    Float         = 8,
    TextSelection = 9,
    String        = 10,
    Folder        = 11,
    Info          = 12,
    Command       = 13,
    Vtx           = 15,
    Unsupported   = 0xFF,   // anything we do not know how to edit
};

// Status byte of a Command field (Bind, WiFi update, ...). Written by us to
// start / confirm / cancel; reported back by the device while it runs.
enum class CommandStatus : uint8_t {
    Ready              = 0,
    Start              = 1,
    Progress           = 2,
    ConfirmationNeeded = 3,
    Confirm            = 4,
    Cancel             = 5,
    Poll               = 6,
};

struct DeviceInfo {
    std::string name;
    std::array<uint8_t, 4> serial{};    // "ELRS" for ExpressLRS devices
    std::array<uint8_t, 4> hardware{};
    std::array<uint8_t, 4> firmware{};
    uint8_t fieldCount      = 0;        // parameters are numbered 1..fieldCount
    uint8_t protocolVersion = 0;
};

struct Field {
    uint8_t     id     = 0;
    uint8_t     parent = 0;             // 0 = top level
    FieldType   type   = FieldType::Unsupported;
    bool        hidden = false;         // device currently hides this entry
    std::string name;

    // Numeric-ish types: ints, float (value/min/max are the raw scaled ints),
    // text selection (value = selected option index).
    int32_t     value = 0;
    int32_t     min   = 0;
    int32_t     max   = 0;
    int32_t     def   = 0;
    std::string unit;

    std::vector<std::string> options;   // TextSelection: index == option value
    uint8_t     decimals = 0;           // Float: value / 10^decimals
    int32_t     step     = 1;           // Float, in raw units

    std::string text;                   // String / Info value; Command info text
    uint8_t     maxLen = 0;             // String

    CommandStatus status      = CommandStatus::Ready;   // Command
    uint8_t       timeout10ms = 0;                      // Command poll interval hint

    std::vector<uint8_t> children;      // Folder: child ids as listed by the device
    bool loaded = false;                // false until the entry has been decoded
};

// One 0x2B parameter-entry chunk, header stripped.
struct EntryChunk {
    uint8_t              fieldId         = 0;
    uint8_t              chunksRemaining = 0;
    std::vector<uint8_t> data;
};

// ---- Encoding (frames ready to write to the serial port) -------------------
// All frames are extended frames from CRSF_ADDRESS_HANDSET.
// Return an empty vector if the payload would not fit in a CRSF frame.

std::vector<uint8_t> buildPing(uint8_t dest);
std::vector<uint8_t> buildParamRead(uint8_t dest, uint8_t fieldId, uint8_t chunk);
std::vector<uint8_t> buildParamWrite(uint8_t dest, uint8_t fieldId,
                                     const std::vector<uint8_t>& value);

// Value bytes for buildParamWrite().
std::vector<uint8_t> encodeNumber(FieldType type, int32_t value);   // ints, float, selection
std::vector<uint8_t> encodeText(const std::string& text);           // chars + NUL
std::vector<uint8_t> encodeCommand(CommandStatus status);
// Wire bytes that would set the field to the value it currently holds; empty
// for types that are not writable. Used to verify a write took effect.
std::vector<uint8_t> currentValueBytes(const Field& field);

// ---- Decoding (payload = bytes after the [dest][origin] header) ------------

bool decodeDeviceInfo(const uint8_t* body, size_t len, DeviceInfo& out);
bool decodeEntryChunk(const uint8_t* body, size_t len, EntryChunk& out);
// Decodes the reassembled chunk bytes of one field. Returns false if the data
// is truncated for its declared type.
bool decodeField(uint8_t id, const uint8_t* data, size_t len, Field& out);

// ---- Helpers ---------------------------------------------------------------

const char* fieldTypeName(FieldType type);
// One-line human description ("#3 Packet Rate [TextSelection] = 2 ..."), for
// the field-tree log dump used during bench bring-up.
std::string describeField(const Field& field);

} // namespace crsf
