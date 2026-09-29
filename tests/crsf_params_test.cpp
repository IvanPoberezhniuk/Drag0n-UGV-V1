// Unit tests for the CRSF device-parameter codec (src/crsf/CrsfParams.*).
// Plain asserts, no framework, no Qt: build with -DUGV_BUILD_TESTS=ON and run
// crsf_params_test; exit code 0 means every check passed.
//
// Reference frame bytes were generated with an independent CRC-8/DVB-S2
// implementation (poly 0xD5), not with the code under test. The bind frame
// below is the one the app already sends to the Nomad on connect.
#include "crsf/CrsfPacket.h"
#include "crsf/CrsfParams.h"
#include "crsf/CrsfTypes.h"
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using Bytes = std::vector<uint8_t>;
using namespace crsf;

static int g_failures = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

// ---- helpers to assemble field bodies --------------------------------------

static void putStr(Bytes& b, const std::string& s) {
    b.insert(b.end(), s.begin(), s.end());
    b.push_back(0);
}
static void putBe16(Bytes& b, int v) {
    b.push_back(static_cast<uint8_t>(v >> 8));
    b.push_back(static_cast<uint8_t>(v));
}
static void putBe32(Bytes& b, int32_t v) {
    const uint32_t u = static_cast<uint32_t>(v);
    for (int shift = 24; shift >= 0; shift -= 8) b.push_back(static_cast<uint8_t>(u >> shift));
}
static Bytes head(uint8_t parent, uint8_t type, const std::string& name) {
    Bytes b{parent, type};
    putStr(b, name);
    return b;
}

// ---- frame builders --------------------------------------------------------

static void testBuilders() {
    // The reference CRC agrees with the bind frame the app already sends.
    const auto bind = buildTxBindCommandPacket();
    CHECK((Bytes(bind.begin(), bind.end()) ==
           Bytes{0xC8, 0x06, 0x32, 0xEE, 0xEA, 0x10, 0x01, 0x8E}));

    CHECK((buildPing(CRSF_ADDRESS_TX_MODULE) == Bytes{0xC8, 0x04, 0x28, 0xEE, 0xEA, 0x97}));
    CHECK((buildPing(CRSF_ADDRESS_RECEIVER)  == Bytes{0xC8, 0x04, 0x28, 0xEC, 0xEA, 0x81}));
    CHECK((buildParamRead(0xEE, 3, 0) ==
           Bytes{0xC8, 0x06, 0x2C, 0xEE, 0xEA, 0x03, 0x00, 0x90}));
    CHECK((buildParamRead(0xEC, 7, 2) ==
           Bytes{0xC8, 0x06, 0x2C, 0xEC, 0xEA, 0x07, 0x02, 0x49}));
    CHECK((buildParamWrite(0xEE, 3, encodeNumber(FieldType::TextSelection, 2)) ==
           Bytes{0xC8, 0x06, 0x2D, 0xEE, 0xEA, 0x03, 0x02, 0x59}));
    CHECK((buildParamWrite(0xEC, 4, encodeText("abc")) ==
           Bytes{0xC8, 0x09, 0x2D, 0xEC, 0xEA, 0x04, 0x61, 0x62, 0x63, 0x00, 0x5E}));
    CHECK((buildParamWrite(0xEE, 9, encodeNumber(FieldType::Int16, -100)) ==
           Bytes{0xC8, 0x07, 0x2D, 0xEE, 0xEA, 0x09, 0xFF, 0x9C, 0x02}));

    // Every frame: length byte covers type..crc, and the CRC checks out the same
    // way CrsfFrameParser verifies it.
    const std::vector<Bytes> frames = {
        buildPing(0x00), buildPing(0xEE), buildParamRead(0xEC, 200, 5),
        buildParamWrite(0xEE, 1, Bytes(20, 0x55)),
    };
    for (const auto& f : frames) {
        CHECK(f.size() >= 6);
        CHECK(f[0] == CRSF_SYNC);
        CHECK(f.size() == static_cast<size_t>(f[1]) + 2u);
        CHECK(crc8_dvbs2(f.data() + 2, f[1] - 1u) == f.back());
    }

    // Largest write that still fits a 64-byte CRSF frame, and one byte more.
    CHECK(!buildParamWrite(0xEE, 1, Bytes(57, 0)).empty());
    CHECK(buildParamWrite(0xEE, 1, Bytes(58, 0)).empty());
}

// ---- device info / entry chunk ---------------------------------------------

static void testDeviceInfo() {
    Bytes body;
    putStr(body, "Nomad");
    body.insert(body.end(), {'E', 'L', 'R', 'S',  0, 0, 0, 0,  0, 3, 4, 1});
    body.push_back(27);   // parameter count
    body.push_back(0);    // protocol version

    DeviceInfo info;
    CHECK(decodeDeviceInfo(body.data(), body.size(), info));
    CHECK(info.name == "Nomad");
    CHECK(info.serial[0] == 'E' && info.serial[3] == 'S');
    CHECK(info.firmware[1] == 3 && info.firmware[2] == 4 && info.firmware[3] == 1);
    CHECK(info.fieldCount == 27);
    CHECK(info.protocolVersion == 0);

    DeviceInfo untouched;
    untouched.name = "keep";
    CHECK(!decodeDeviceInfo(body.data(), body.size() - 1, untouched));   // truncated
    CHECK(untouched.name == "keep");
    CHECK(!decodeDeviceInfo(body.data(), 0, untouched));
}

static void testEntryChunk() {
    const Bytes body{5, 1, 0xAA, 0xBB};
    EntryChunk chunk;
    CHECK(decodeEntryChunk(body.data(), body.size(), chunk));
    CHECK(chunk.fieldId == 5);
    CHECK(chunk.chunksRemaining == 1);
    CHECK((chunk.data == Bytes{0xAA, 0xBB}));

    CHECK(decodeEntryChunk(body.data(), 2, chunk));   // header only: empty chunk data
    CHECK(chunk.data.empty());
    CHECK(!decodeEntryChunk(body.data(), 1, chunk));
    CHECK(!decodeEntryChunk(nullptr, 0, chunk));
}

// ---- field decoding, one per type ------------------------------------------

static void testFieldTypes() {
    Field f;

    {   // Text selection
        Bytes b = head(0, 9, "Packet Rate");
        putStr(b, "50Hz;100Hz;250Hz");
        b.insert(b.end(), {1, 0, 2, 1});
        putStr(b, "");
        CHECK(decodeField(4, b.data(), b.size(), f));
        CHECK(f.id == 4 && f.parent == 0 && f.type == FieldType::TextSelection);
        CHECK(f.name == "Packet Rate");
        CHECK(f.options.size() == 3 && f.options[0] == "50Hz" && f.options[2] == "250Hz");
        CHECK(f.value == 1 && f.min == 0 && f.max == 2 && f.def == 1);
        CHECK(f.unit.empty() && f.loaded && !f.hidden);
    }
    {   // Uint8 with a unit, inside a folder
        Bytes b = head(3, 0, "Level");
        b.insert(b.end(), {5, 0, 10, 3});
        putStr(b, "dBm");
        CHECK(decodeField(7, b.data(), b.size(), f));
        CHECK(f.parent == 3 && f.type == FieldType::Uint8);
        CHECK(f.value == 5 && f.min == 0 && f.max == 10 && f.def == 3 && f.unit == "dBm");
    }
    {   // Int8 sign extension
        Bytes b = head(0, 1, "Trim");
        b.insert(b.end(), {0xF6, 0x80, 0x7F, 0});   // -10, -128, 127, 0
        putStr(b, "");
        CHECK(decodeField(1, b.data(), b.size(), f));
        CHECK(f.type == FieldType::Int8 && f.value == -10 && f.min == -128 && f.max == 127);
    }
    {   // Int16 negative, big-endian
        Bytes b = head(0, 3, "Offset");
        putBe16(b, -100); putBe16(b, -200); putBe16(b, 100); putBe16(b, 0);
        putStr(b, "");
        CHECK(decodeField(2, b.data(), b.size(), f));
        CHECK(f.type == FieldType::Int16 && f.value == -100 && f.min == -200 && f.max == 100);
    }
    {   // Uint16 is not sign-extended
        Bytes b = head(0, 2, "Big");
        putBe16(b, 1000); putBe16(b, 0); putBe16(b, 65535); putBe16(b, 500);
        putStr(b, "ms");
        CHECK(decodeField(2, b.data(), b.size(), f));
        CHECK(f.type == FieldType::Uint16 && f.value == 1000 && f.max == 65535 && f.unit == "ms");
    }
    {   // Float
        Bytes b = head(0, 8, "Volts");
        putBe32(b, 1234); putBe32(b, 0); putBe32(b, 5000); putBe32(b, 1000);
        b.push_back(2);        // decimal point
        putBe32(b, 10);        // step
        putStr(b, "V");
        CHECK(decodeField(2, b.data(), b.size(), f));
        CHECK(f.type == FieldType::Float && f.value == 1234 && f.max == 5000);
        CHECK(f.decimals == 2 && f.step == 10 && f.unit == "V");
    }
    {   // String, with and without the trailing max length
        Bytes b = head(0, 10, "Name");
        putStr(b, "abc");
        b.push_back(16);
        CHECK(decodeField(2, b.data(), b.size(), f));
        CHECK(f.type == FieldType::String && f.text == "abc" && f.maxLen == 16);

        Bytes noMax = head(0, 10, "Name");
        putStr(noMax, "abc");
        CHECK(decodeField(2, noMax.data(), noMax.size(), f));
        CHECK(f.text == "abc" && f.maxLen == 0);
    }
    {   // Folder
        Bytes b = head(0, 11, "WiFi");
        b.insert(b.end(), {8, 9, 10, 0xFF});
        CHECK(decodeField(6, b.data(), b.size(), f));
        CHECK(f.type == FieldType::Folder && f.children.size() == 3 && f.children[2] == 10);
    }
    {   // Info
        Bytes b = head(0, 12, "Version");
        putStr(b, "3.4.1");
        CHECK(decodeField(9, b.data(), b.size(), f));
        CHECK(f.type == FieldType::Info && f.text == "3.4.1");
    }
    {   // Command, idle and asking for confirmation
        Bytes idle = head(0, 13, "Bind");
        idle.insert(idle.end(), {0, 20});
        putStr(idle, "");
        CHECK(decodeField(5, idle.data(), idle.size(), f));
        CHECK(f.type == FieldType::Command && f.status == CommandStatus::Ready);
        CHECK(f.timeout10ms == 20 && f.text.empty());

        Bytes ask = head(0, 13, "Bind");
        ask.insert(ask.end(), {3, 20});
        putStr(ask, "Really bind?");
        CHECK(decodeField(5, ask.data(), ask.size(), f));
        CHECK(f.status == CommandStatus::ConfirmationNeeded && f.text == "Really bind?");
    }
    {   // Hidden flag is bit 7 of the type byte
        Bytes b = head(0, 0x89, "Hidden");
        putStr(b, "a;b");
        b.insert(b.end(), {0, 0, 1, 0});
        putStr(b, "");
        CHECK(decodeField(2, b.data(), b.size(), f));
        CHECK(f.hidden && f.type == FieldType::TextSelection);
    }
    {   // Types we cannot edit still decode by name
        Bytes vtx = head(0, 15, "VTX");
        vtx.insert(vtx.end(), {1, 2, 3});
        CHECK(decodeField(2, vtx.data(), vtx.size(), f));
        CHECK(f.type == FieldType::Vtx && f.name == "VTX" && f.loaded);

        const Bytes weird = head(0, 0x7F, "Odd");
        CHECK(decodeField(2, weird.data(), weird.size(), f));
        CHECK(f.type == FieldType::Unsupported);
    }
}

static void testTruncatedFields() {
    Field f;
    f.name = "keep";

    CHECK(!decodeField(1, nullptr, 0, f));
    const Bytes tiny{0, 9};   // no name
    CHECK(!decodeField(1, tiny.data(), tiny.size(), f));

    // Text selection cut off before its value bytes.
    Bytes cut = head(0, 9, "Packet Rate");
    putStr(cut, "a;b");
    cut.insert(cut.end(), {1, 0});   // needs 4 bytes
    CHECK(!decodeField(1, cut.data(), cut.size(), f));

    // Int16 cut in the middle of a value.
    Bytes cut16 = head(0, 3, "Offset");
    putBe16(cut16, 5);
    cut16.push_back(0);
    CHECK(!decodeField(1, cut16.data(), cut16.size(), f));

    CHECK(f.name == "keep");   // a failed decode leaves the output alone
}

// ---- encoding / round trip -------------------------------------------------

static void testEncoders() {
    CHECK((encodeNumber(FieldType::TextSelection, 3) == Bytes{3}));
    CHECK((encodeNumber(FieldType::Uint8, 200) == Bytes{200}));
    CHECK((encodeNumber(FieldType::Int8, -10) == Bytes{0xF6}));
    CHECK((encodeNumber(FieldType::Uint16, 1000) == Bytes{0x03, 0xE8}));
    CHECK((encodeNumber(FieldType::Int16, -100) == Bytes{0xFF, 0x9C}));
    CHECK((encodeNumber(FieldType::Float, 1234) == Bytes{0x00, 0x00, 0x04, 0xD2}));
    CHECK(encodeNumber(FieldType::Folder, 1).empty());
    CHECK((encodeText("abc") == Bytes{'a', 'b', 'c', 0}));
    CHECK((encodeText("") == Bytes{0}));
    CHECK((encodeCommand(CommandStatus::Start) == Bytes{1}));
    CHECK((encodeCommand(CommandStatus::Confirm) == Bytes{4}));
    CHECK((encodeCommand(CommandStatus::Cancel) == Bytes{5}));

    // currentValueBytes(): what would re-write the value the field holds now.
    Field f;
    Bytes sel = head(0, 9, "Rate");
    putStr(sel, "a;b;c");
    sel.insert(sel.end(), {2, 0, 2, 0});
    putStr(sel, "");
    CHECK(decodeField(1, sel.data(), sel.size(), f));
    CHECK((currentValueBytes(f) == Bytes{2}));

    Bytes i16 = head(0, 3, "Offset");
    putBe16(i16, -100); putBe16(i16, -200); putBe16(i16, 100); putBe16(i16, 0);
    putStr(i16, "");
    CHECK(decodeField(1, i16.data(), i16.size(), f));
    CHECK((currentValueBytes(f) == encodeNumber(FieldType::Int16, -100)));

    Bytes str = head(0, 10, "Name");
    putStr(str, "abc");
    CHECK(decodeField(1, str.data(), str.size(), f));
    CHECK((currentValueBytes(f) == encodeText("abc")));

    Bytes folder = head(0, 11, "Dir");
    folder.push_back(0xFF);
    CHECK(decodeField(1, folder.data(), folder.size(), f));
    CHECK(currentValueBytes(f).empty());
}

static void testDescribe() {
    Field f;
    Bytes b = head(0, 9, "Packet Rate");
    putStr(b, "50Hz;100Hz");
    b.insert(b.end(), {1, 0, 1, 0});
    putStr(b, "");
    CHECK(decodeField(4, b.data(), b.size(), f));
    const std::string text = describeField(f);
    CHECK(text.find("Packet Rate") != std::string::npos);
    CHECK(text.find("100Hz") != std::string::npos);
    CHECK(std::string(fieldTypeName(FieldType::Command)) == "Command");
    CHECK(describeField(Field{}).find("not read") != std::string::npos);
}

int main() {
    testBuilders();
    testDeviceInfo();
    testEntryChunk();
    testFieldTypes();
    testTruncatedFields();
    testEncoders();
    testDescribe();

    if (g_failures == 0) {
        std::printf("crsf_params_test: all checks passed\n");
        return 0;
    }
    std::printf("crsf_params_test: %d check(s) FAILED\n", g_failures);
    return 1;
}
