#include "crsf/CrsfParams.h"
#include "crsf/CrsfPacket.h"
#include "crsf/CrsfTypes.h"
#include <utility>

namespace crsf {

namespace {

// CRSF frames are at most 64 bytes: [sync][len] + len bytes, so len <= 62.
constexpr size_t kMaxLenField    = 62;
// len counts type + dest + origin + body + crc.
constexpr size_t kExtendedFixed  = 4;

// Bounds-checked cursor over a received byte range. Any read past the end
// sets ok() to false and yields zeros, so decoders can read straight through
// and check ok() once at the end.
class Reader {
public:
    Reader(const uint8_t* data, size_t len) : m_data(data), m_len(len) {}

    bool   ok() const        { return m_ok; }
    size_t remaining() const { return m_len - m_pos; }

    uint8_t u8() {
        if (m_pos >= m_len) { m_ok = false; return 0; }
        return m_data[m_pos++];
    }

    // Big-endian integer of `size` (1, 2 or 4) bytes, sign-extended on request.
    int32_t be(size_t size, bool isSigned) {
        uint32_t v = 0;
        for (size_t i = 0; i < size; ++i) v = (v << 8) | u8();
        if (!isSigned) return static_cast<int32_t>(v);
        switch (size) {
            case 1:  return static_cast<int8_t>(v);
            case 2:  return static_cast<int16_t>(v);
            default: return static_cast<int32_t>(v);
        }
    }

    // NUL-terminated string. Running out of data before the NUL is tolerated
    // (returns what was read), so a trailing empty "unit" string may be omitted.
    std::string cstr() {
        std::string s;
        while (m_pos < m_len) {
            const uint8_t c = m_data[m_pos++];
            if (c == 0) break;
            s.push_back(static_cast<char>(c));
        }
        return s;
    }

private:
    const uint8_t* m_data;
    size_t         m_len;
    size_t         m_pos = 0;
    bool           m_ok  = true;
};

FieldType toFieldType(uint8_t raw) {
    switch (raw) {
        case 0:  return FieldType::Uint8;
        case 1:  return FieldType::Int8;
        case 2:  return FieldType::Uint16;
        case 3:  return FieldType::Int16;
        case 8:  return FieldType::Float;
        case 9:  return FieldType::TextSelection;
        case 10: return FieldType::String;
        case 11: return FieldType::Folder;
        case 12: return FieldType::Info;
        case 13: return FieldType::Command;
        case 15: return FieldType::Vtx;
        default: return FieldType::Unsupported;
    }
}

std::vector<std::string> splitOptions(const std::string& s) {
    std::vector<std::string> out;
    if (s.empty()) return out;
    std::string cur;
    for (char c : s) {
        if (c == ';') { out.push_back(cur); cur.clear(); }
        else          { cur.push_back(c); }
    }
    out.push_back(cur);
    return out;
}

// [sync][len][type][dest][origin=handset][body...][crc]
std::vector<uint8_t> buildExtended(uint8_t type, uint8_t dest,
                                   const std::vector<uint8_t>& body) {
    const size_t lenField = body.size() + kExtendedFixed;
    if (lenField > kMaxLenField) return {};

    std::vector<uint8_t> frame;
    frame.reserve(lenField + 2);
    frame.push_back(CRSF_SYNC);
    frame.push_back(static_cast<uint8_t>(lenField));
    frame.push_back(type);
    frame.push_back(dest);
    frame.push_back(CRSF_ADDRESS_HANDSET);
    frame.insert(frame.end(), body.begin(), body.end());
    // CRC covers type .. last body byte (everything after sync + len).
    frame.push_back(crc8_dvbs2(&frame[2], frame.size() - 2));
    return frame;
}

} // namespace

// ---- Encoding --------------------------------------------------------------

std::vector<uint8_t> buildPing(uint8_t dest) {
    return buildExtended(CRSF_FRAMETYPE_DEVICE_PING, dest, {});
}

std::vector<uint8_t> buildParamRead(uint8_t dest, uint8_t fieldId, uint8_t chunk) {
    return buildExtended(CRSF_FRAMETYPE_PARAM_READ, dest, {fieldId, chunk});
}

std::vector<uint8_t> buildParamWrite(uint8_t dest, uint8_t fieldId,
                                     const std::vector<uint8_t>& value) {
    std::vector<uint8_t> body;
    body.reserve(value.size() + 1);
    body.push_back(fieldId);
    body.insert(body.end(), value.begin(), value.end());
    return buildExtended(CRSF_FRAMETYPE_PARAM_WRITE, dest, body);
}

std::vector<uint8_t> encodeNumber(FieldType type, int32_t value) {
    const uint32_t v = static_cast<uint32_t>(value);
    switch (type) {
        case FieldType::Uint8:
        case FieldType::Int8:
        case FieldType::TextSelection:
            return { static_cast<uint8_t>(v) };
        case FieldType::Uint16:
        case FieldType::Int16:
            return { static_cast<uint8_t>(v >> 8), static_cast<uint8_t>(v) };
        case FieldType::Float:
            return { static_cast<uint8_t>(v >> 24), static_cast<uint8_t>(v >> 16),
                     static_cast<uint8_t>(v >> 8),  static_cast<uint8_t>(v) };
        default:
            return {};
    }
}

std::vector<uint8_t> encodeText(const std::string& text) {
    std::vector<uint8_t> out(text.begin(), text.end());
    out.push_back(0);
    return out;
}

std::vector<uint8_t> encodeCommand(CommandStatus status) {
    return { static_cast<uint8_t>(status) };
}

std::vector<uint8_t> currentValueBytes(const Field& field) {
    switch (field.type) {
        case FieldType::Uint8:
        case FieldType::Int8:
        case FieldType::Uint16:
        case FieldType::Int16:
        case FieldType::Float:
        case FieldType::TextSelection:
            return encodeNumber(field.type, field.value);
        case FieldType::String:
            return encodeText(field.text);
        default:
            return {};
    }
}

// ---- Decoding --------------------------------------------------------------

bool decodeDeviceInfo(const uint8_t* body, size_t len, DeviceInfo& out) {
    Reader r(body, len);
    DeviceInfo info;
    info.name = r.cstr();
    for (auto& b : info.serial)   b = r.u8();
    for (auto& b : info.hardware) b = r.u8();
    for (auto& b : info.firmware) b = r.u8();
    info.fieldCount      = r.u8();
    info.protocolVersion = r.u8();
    if (!r.ok()) return false;
    out = std::move(info);
    return true;
}

bool decodeEntryChunk(const uint8_t* body, size_t len, EntryChunk& out) {
    if (body == nullptr || len < 2) return false;
    out.fieldId         = body[0];
    out.chunksRemaining = body[1];
    out.data.assign(body + 2, body + len);
    return true;
}

bool decodeField(uint8_t id, const uint8_t* data, size_t len, Field& out) {
    Reader r(data, len);
    Field f;
    f.id = id;
    f.parent = r.u8();
    const uint8_t typeByte = r.u8();
    f.hidden = (typeByte & 0x80u) != 0;
    f.type   = toFieldType(typeByte & 0x7Fu);
    f.name   = r.cstr();
    if (!r.ok()) return false;

    switch (f.type) {
        case FieldType::Uint8:
        case FieldType::Int8:
        case FieldType::Uint16:
        case FieldType::Int16: {
            const size_t size = (f.type == FieldType::Uint8 || f.type == FieldType::Int8) ? 1u : 2u;
            const bool isSigned = (f.type == FieldType::Int8 || f.type == FieldType::Int16);
            f.value = r.be(size, isSigned);
            f.min   = r.be(size, isSigned);
            f.max   = r.be(size, isSigned);
            f.def   = r.be(size, isSigned);
            f.unit  = r.cstr();
            break;
        }
        case FieldType::Float:
            f.value    = r.be(4, true);
            f.min      = r.be(4, true);
            f.max      = r.be(4, true);
            f.def      = r.be(4, true);
            f.decimals = r.u8();
            f.step     = r.be(4, true);
            f.unit     = r.cstr();
            break;
        case FieldType::TextSelection:
            f.options = splitOptions(r.cstr());
            f.value   = r.u8();
            f.min     = r.u8();
            f.max     = r.u8();
            f.def     = r.u8();
            f.unit    = r.cstr();
            break;
        case FieldType::String:
            f.text = r.cstr();
            if (r.remaining() > 0) f.maxLen = r.u8();
            break;
        case FieldType::Folder:
            while (r.remaining() > 0) {
                const uint8_t child = r.u8();
                if (child == 0xFF) break;
                f.children.push_back(child);
            }
            break;
        case FieldType::Info:
            f.text = r.cstr();
            break;
        case FieldType::Command:
            f.status      = static_cast<CommandStatus>(r.u8());
            f.timeout10ms = r.u8();
            f.text        = r.cstr();
            break;
        case FieldType::Vtx:
        case FieldType::Unsupported:
            break;   // shown read-only by name only
    }

    if (!r.ok()) return false;
    f.loaded = true;
    out = std::move(f);
    return true;
}

// ---- Helpers ---------------------------------------------------------------

const char* fieldTypeName(FieldType type) {
    switch (type) {
        case FieldType::Uint8:         return "Uint8";
        case FieldType::Int8:          return "Int8";
        case FieldType::Uint16:        return "Uint16";
        case FieldType::Int16:         return "Int16";
        case FieldType::Float:         return "Float";
        case FieldType::TextSelection: return "TextSelection";
        case FieldType::String:        return "String";
        case FieldType::Folder:        return "Folder";
        case FieldType::Info:          return "Info";
        case FieldType::Command:       return "Command";
        case FieldType::Vtx:           return "Vtx";
        case FieldType::Unsupported:   return "Unsupported";
    }
    return "?";
}

std::string describeField(const Field& f) {
    std::string s = "#" + std::to_string(f.id) + " parent=" + std::to_string(f.parent) +
                    " [" + fieldTypeName(f.type) + "] '" + f.name + "'";
    if (f.hidden) s += " (hidden)";
    if (!f.loaded) return s + " (not read)";

    const std::string range = " range " + std::to_string(f.min) + ".." + std::to_string(f.max);
    const std::string unit  = f.unit.empty() ? std::string() : " " + f.unit;
    switch (f.type) {
        case FieldType::Uint8:
        case FieldType::Int8:
        case FieldType::Uint16:
        case FieldType::Int16:
            s += " = " + std::to_string(f.value) + unit + range;
            break;
        case FieldType::Float:
            s += " = " + std::to_string(f.value) + " (x10^-" + std::to_string(f.decimals) + ")" +
                 unit + range;
            break;
        case FieldType::TextSelection: {
            s += " = " + std::to_string(f.value) + unit + " options [";
            for (size_t i = 0; i < f.options.size(); ++i) {
                if (i != 0) s += ", ";
                s += std::to_string(i) + ":" + f.options[i];
            }
            s += "]" + range;
            break;
        }
        case FieldType::String:
        case FieldType::Info:
            s += " = '" + f.text + "'";
            break;
        case FieldType::Command:
            s += " status=" + std::to_string(static_cast<int>(f.status)) +
                 " timeout=" + std::to_string(f.timeout10ms * 10) + "ms info='" + f.text + "'";
            break;
        case FieldType::Folder:
            s += " children=" + std::to_string(f.children.size());
            break;
        case FieldType::Vtx:
        case FieldType::Unsupported:
            break;
    }
    return s;
}

} // namespace crsf
