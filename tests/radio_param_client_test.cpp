// Behavioural tests for RadioParamClient (src/io/RadioParamClient.*), driven
// against a simulated ELRS TX module and receiver: chunked entries, dropped
// replies (retry), dead devices, writes that do not take, armed write-discard,
// and the command / confirmation flow. No Qt, no serial port, no hardware.
//
// Build with -DUGV_BUILD_TESTS=ON and run radio_param_client_test; exit code 0
// means every check passed. The simulator runs on a virtual clock (20 ms per
// step, like the 50 Hz RC loop that calls nextFrame() in the app).
#include "core/ChronoTypes.h"
#include "crsf/CrsfPacket.h"
#include "crsf/CrsfParams.h"
#include "crsf/CrsfTypes.h"
#include "io/RadioParamClient.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using Bytes  = std::vector<uint8_t>;
using Status = RadioParamClient::DeviceStatus;

static int g_failures = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

static void putStr(Bytes& b, const std::string& s) {
    b.insert(b.end(), s.begin(), s.end());
    b.push_back(0);
}

// ---- simulated device ------------------------------------------------------

struct SimField {
    Bytes  body;                 // reassembled entry: [parent][type][name\0]...
    size_t valueOffset = 0;      // where the value / status byte lives in body
    bool   isCommand   = false;
};

static SimField makeSelect(uint8_t parent, const std::string& name,
                           const std::string& options, uint8_t value, uint8_t max) {
    SimField f;
    f.body = {parent, 9};
    putStr(f.body, name);
    putStr(f.body, options);
    f.valueOffset = f.body.size();
    f.body.insert(f.body.end(), {value, 0, max, 0});
    putStr(f.body, "");
    return f;
}

static SimField makeUint8(uint8_t parent, const std::string& name, uint8_t value,
                          uint8_t max, const std::string& unit) {
    SimField f;
    f.body = {parent, 0};
    putStr(f.body, name);
    f.valueOffset = f.body.size();
    f.body.insert(f.body.end(), {value, 0, max, value});
    putStr(f.body, unit);
    return f;
}

static SimField makeFolder(const std::string& name) {
    SimField f;
    f.body = {0, 11};
    putStr(f.body, name);
    f.body.push_back(0xFF);
    return f;
}

static SimField makeInfo(const std::string& name, const std::string& text) {
    SimField f;
    f.body = {0, 12};
    putStr(f.body, name);
    putStr(f.body, text);
    return f;
}

static SimField makeCommand(const std::string& name, const std::string& info) {
    SimField f;
    f.body = {0, 13};
    putStr(f.body, name);
    f.valueOffset = f.body.size();
    f.body.insert(f.body.end(), {0, 20});
    putStr(f.body, info);
    f.isCommand = true;
    return f;
}

struct SimDevice {
    uint8_t              address = 0;
    std::string          name;
    std::vector<SimField> fields;
    bool                 alive = true;
    int                  dropReplies = 0;     // swallow this many upcoming replies
    bool                 ignoreWrites = false;
    bool                 commandNeedsConfirm = true;
    size_t               chunkSize = 24;      // small, so most entries span chunks
    int                  progressLeft = 0;
};

struct Sim {
    RadioParamClient client;
    SimDevice tx;
    SimDevice rx;
    std::vector<std::string> events;          // "read:EE:3", "write:EC:1", "cmd:EE:5:4"
    Clock::time_point now = Clock::now();
    bool writesAllowed = true;

    Sim() {
        tx.address = CRSF_ADDRESS_TX_MODULE;
        tx.name    = "Nomad";
        tx.fields  = {
            makeSelect(0, "Packet Rate", "50Hz;100Hz;250Hz;500Hz", 1, 3),       // 1
            makeSelect(0, "Telem Ratio", "Off;1:128;1:64;1:32;1:16;1:8", 3, 5), // 2
            makeFolder("TX Power"),                                             // 3
            makeUint8(3, "Max Power", 100, 250, "mW"),                          // 4
            makeCommand("Bind", "Bind now?"),                                   // 5
            makeInfo("Version", "3.4.1"),                                       // 6
        };
        rx.address = CRSF_ADDRESS_RECEIVER;
        rx.name    = "XR4";
        rx.fields  = {
            makeSelect(0, "Protocol", "CRSF;Inverted CRSF;SBUS", 0, 2),         // 1
            makeSelect(0, "Failsafe Mode", "No Pulses;Last Pos", 0, 1),         // 2
        };
    }

    SimDevice* find(uint8_t address) {
        if (address == tx.address) return &tx;
        if (address == rx.address) return &rx;
        return nullptr;
    }

    static Bytes infoBody(const SimDevice& d) {
        Bytes b;
        putStr(b, d.name);
        b.insert(b.end(), {'E', 'L', 'R', 'S', 0, 0, 0, 0, 0, 3, 4, 1});
        b.push_back(static_cast<uint8_t>(d.fields.size()));
        b.push_back(0);
        return b;
    }

    void reply(SimDevice& d, uint8_t type, const Bytes& body) {
        if (d.dropReplies > 0) { --d.dropReplies; return; }
        client.onFrame(type, d.address, body.data(), body.size());
    }

    void deliver(const Bytes& frame) {
        // Every frame the client emits must be a well-formed extended frame.
        CHECK(frame.size() >= 6);
        if (frame.size() < 6) return;
        CHECK(frame[0] == CRSF_SYNC);
        CHECK(frame.size() == static_cast<size_t>(frame[1]) + 2u);
        CHECK(crc8_dvbs2(frame.data() + 2, frame[1] - 1u) == frame.back());
        CHECK(frame[4] == CRSF_ADDRESS_HANDSET);

        const uint8_t type = frame[2];
        SimDevice* d = find(frame[3]);
        if (d == nullptr || !d->alive) return;
        const uint8_t* body = frame.data() + 5;
        const size_t len = frame.size() - 6;
        char tag[32];

        if (type == CRSF_FRAMETYPE_DEVICE_PING) {
            reply(*d, CRSF_FRAMETYPE_DEVICE_INFO, infoBody(*d));
        } else if (type == CRSF_FRAMETYPE_PARAM_READ && len >= 2) {
            const size_t id = body[0], chunk = body[1];
            std::snprintf(tag, sizeof tag, "read:%02X:%zu", d->address, id);
            events.push_back(tag);
            if (id < 1 || id > d->fields.size()) return;
            SimField& f = d->fields[id - 1];

            const size_t chunks = (f.body.size() + d->chunkSize - 1) / d->chunkSize;
            const size_t from = chunk * d->chunkSize;
            if (from >= f.body.size()) return;
            const size_t to = std::min(from + d->chunkSize, f.body.size());
            Bytes out{static_cast<uint8_t>(id), static_cast<uint8_t>(chunks - 1 - chunk)};
            out.insert(out.end(), f.body.begin() + static_cast<long>(from),
                       f.body.begin() + static_cast<long>(to));
            reply(*d, CRSF_FRAMETYPE_PARAM_ENTRY, out);

            // A running command finishes after a few polls.
            if (f.isCommand && f.body[f.valueOffset] == 2 && --d->progressLeft <= 0)
                f.body[f.valueOffset] = 0;
        } else if (type == CRSF_FRAMETYPE_PARAM_WRITE && len >= 2) {
            const size_t id = body[0];
            if (id < 1 || id > d->fields.size()) return;
            SimField& f = d->fields[id - 1];
            if (f.isCommand) {
                std::snprintf(tag, sizeof tag, "cmd:%02X:%zu:%d", d->address, id, body[1]);
                events.push_back(tag);
                switch (body[1]) {
                    case 1:  f.body[f.valueOffset] = d->commandNeedsConfirm ? 3 : 2;
                             d->progressLeft = 2; break;
                    case 4:  f.body[f.valueOffset] = 2; d->progressLeft = 2; break;
                    case 5:  f.body[f.valueOffset] = 0; break;
                    default: break;
                }
            } else {
                std::snprintf(tag, sizeof tag, "write:%02X:%zu", d->address, id);
                events.push_back(tag);
                if (!d->ignoreWrites) f.body[f.valueOffset] = body[1];
            }
        }
    }

    // Steps the virtual clock until the client is idle (or, optionally, until a
    // confirmation is pending). Returns false if it never settled.
    bool run(int maxSteps = 4000, bool stopOnConfirm = false) {
        for (int i = 0; i < maxSteps; ++i) {
            if (auto frame = client.nextFrame(now, writesAllowed)) deliver(*frame);
            now += Ms(20);
            const auto snap = client.snapshot();
            if (stopOnConfirm && snap.confirmation.pending) return true;
            if (!snap.busy) return true;
        }
        return false;
    }

    size_t indexOf(const std::string& event) const {
        for (size_t i = 0; i < events.size(); ++i)
            if (events[i] == event) return i;
        return events.size();
    }
    size_t lastIndexOfPrefix(const std::string& prefix) const {
        size_t last = events.size();
        for (size_t i = 0; i < events.size(); ++i)
            if (events[i].rfind(prefix, 0) == 0) last = i;
        return last;
    }
    size_t firstIndexOfPrefix(const std::string& prefix) const {
        for (size_t i = 0; i < events.size(); ++i)
            if (events[i].rfind(prefix, 0) == 0) return i;
        return events.size();
    }
};

static bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

static RadioParamClient::WriteRequest writeReq(uint8_t id, uint8_t value) {
    return { id, crsf::encodeNumber(crsf::FieldType::TextSelection, value) };
}

// ---- tests -----------------------------------------------------------------

static void testFullLoad() {
    Sim sim;
    sim.client.beginLoad();
    CHECK(sim.run());

    const auto snap = sim.client.snapshot();
    const auto& tx = snap.devices[0];
    const auto& rx = snap.devices[1];
    CHECK(tx.status == Status::Ready);
    CHECK(rx.status == Status::Ready);
    CHECK(tx.info.name == "Nomad" && rx.info.name == "XR4");
    CHECK(tx.fields.size() == 6 && rx.fields.size() == 2);
    CHECK(tx.generation != 0 && rx.generation != 0 && tx.generation != rx.generation);

    // Multi-chunk entries reassembled correctly.
    CHECK(tx.fields[0].name == "Packet Rate");
    CHECK(tx.fields[0].options.size() == 4 && tx.fields[0].options[3] == "500Hz");
    CHECK(tx.fields[0].value == 1 && tx.fields[0].max == 3);
    CHECK(tx.fields[1].options.size() == 6 && tx.fields[1].value == 3);
    CHECK(tx.fields[2].type == crsf::FieldType::Folder);
    CHECK(tx.fields[3].parent == 3 && tx.fields[3].unit == "mW" && tx.fields[3].value == 100);
    CHECK(tx.fields[4].type == crsf::FieldType::Command && tx.fields[4].text == "Bind now?");
    CHECK(tx.fields[5].text == "3.4.1");
    CHECK(rx.fields[1].name == "Failsafe Mode" && rx.fields[1].options[1] == "Last Pos");
    for (const auto& f : tx.fields) CHECK(f.loaded);

    // The TX module is read completely before the receiver is touched.
    CHECK(sim.lastIndexOfPrefix("read:EE") < sim.firstIndexOfPrefix("read:EC"));
    CHECK(!snap.busy && !snap.writesUnconfirmed && !snap.confirmation.pending);
}

static void testTinyChunks() {
    // Pathologically small chunks: every field spans many chunk requests.
    Sim sim;
    sim.tx.chunkSize = 5;
    sim.rx.chunkSize = 5;
    sim.client.beginLoad();
    CHECK(sim.run());
    const auto snap = sim.client.snapshot();
    CHECK(snap.devices[0].status == Status::Ready);
    CHECK(snap.devices[0].fields[0].options.size() == 4);
    CHECK(snap.devices[1].fields[0].options[2] == "SBUS");
}

static void testRetryAfterDroppedReplies() {
    Sim sim;
    sim.tx.dropReplies = 2;    // ping is answered only on the third attempt
    sim.client.beginLoad();
    CHECK(sim.run());
    CHECK(sim.client.snapshot().devices[0].status == Status::Ready);
    CHECK(sim.client.snapshot().devices[1].status == Status::Ready);
}

static void testIgnoresUnexpectedReplies() {
    Sim sim;
    sim.client.beginLoad();
    auto frame = sim.client.nextFrame(sim.now, true);   // ping to the TX module now in flight
    CHECK(frame.has_value());

    const Bytes entry{1, 0, 0, 0};
    sim.client.onFrame(CRSF_FRAMETYPE_PARAM_ENTRY, CRSF_ADDRESS_TX_MODULE, entry.data(), entry.size());
    const Bytes tooShort{'N'};
    sim.client.onFrame(CRSF_FRAMETYPE_DEVICE_INFO, CRSF_ADDRESS_TX_MODULE, tooShort.data(), tooShort.size());
    const Bytes info = Sim::infoBody(sim.tx);
    sim.client.onFrame(CRSF_FRAMETYPE_DEVICE_INFO, CRSF_ADDRESS_RECEIVER, info.data(), info.size());
    CHECK(sim.client.snapshot().devices[0].status == Status::Pinging);

    CHECK(sim.run());   // the real reply is still accepted afterwards
    CHECK(sim.client.snapshot().devices[0].status == Status::Ready);
}

static void testDeadReceiver() {
    Sim sim;
    sim.rx.alive = false;
    sim.client.beginLoad();
    CHECK(sim.run());
    const auto snap = sim.client.snapshot();
    CHECK(snap.devices[0].status == Status::Ready);
    CHECK(snap.devices[1].status == Status::NoResponse);
    CHECK(contains(snap.message, "receiver"));
    CHECK(!snap.busy);
}

static void testDeadTxModule() {
    Sim sim;
    sim.tx.alive = false;
    sim.client.beginLoad();
    CHECK(sim.run());
    const auto snap = sim.client.snapshot();
    CHECK(snap.devices[0].status == Status::NoResponse);
    CHECK(snap.devices[1].status == Status::NoResponse);   // only reachable through the TX
    CHECK(contains(snap.message, "TX module"));
    CHECK(!snap.busy);
    CHECK(sim.events.empty());
    // Nothing is retried forever: a quiet client sends nothing more.
    CHECK(!sim.client.nextFrame(sim.now + Ms(60000), true).has_value());
}

static void testRefusesUntilReady() {
    Sim sim;
    CHECK(!sim.client.applyWrites({{CRSF_ADDRESS_TX_MODULE, {writeReq(1, 2)}}}));
    CHECK(!sim.client.runCommand(CRSF_ADDRESS_TX_MODULE, 5));

    sim.client.beginLoad();
    CHECK(sim.run());
    CHECK(sim.client.applyWrites({{CRSF_ADDRESS_TX_MODULE, {writeReq(1, 2)}}}));
    // Busy with the first request: a second one is refused, nothing is queued twice.
    CHECK(!sim.client.applyWrites({{CRSF_ADDRESS_TX_MODULE, {writeReq(2, 1)}}}));
    CHECK(!sim.client.runCommand(CRSF_ADDRESS_TX_MODULE, 5));
    CHECK(sim.run());
    // Bad target / bad batches.
    CHECK(!sim.client.applyWrites({}));
    CHECK(!sim.client.applyWrites({{0x77, {writeReq(1, 2)}}}));
    CHECK(!sim.client.applyWrites({{CRSF_ADDRESS_TX_MODULE, {}}}));
    CHECK(!sim.client.runCommand(CRSF_ADDRESS_TX_MODULE, 1));   // not a command field
}

static void testApplyWritesAndVerify() {
    Sim sim;
    sim.client.beginLoad();
    CHECK(sim.run());
    const auto before = sim.client.snapshot();

    // Receiver first, then the TX module (a TX change may interrupt the RF link).
    RadioParamClient::DeviceWrites rxBatch{CRSF_ADDRESS_RECEIVER, {writeReq(1, 1)}};
    RadioParamClient::DeviceWrites txBatch{CRSF_ADDRESS_TX_MODULE, {writeReq(1, 2), writeReq(2, 4)}};
    CHECK(sim.client.applyWrites({rxBatch, txBatch}));
    CHECK(sim.client.snapshot().busy);
    CHECK(sim.run());

    const auto snap = sim.client.snapshot();
    CHECK(snap.devices[0].status == Status::Ready && snap.devices[1].status == Status::Ready);
    CHECK(snap.devices[1].fields[0].value == 1);   // receiver Protocol
    CHECK(snap.devices[0].fields[0].value == 2);   // Packet Rate
    CHECK(snap.devices[0].fields[1].value == 4);   // Telem Ratio
    CHECK(!snap.writesUnconfirmed);
    CHECK(snap.message == "Settings applied and confirmed by the device.");
    CHECK(snap.devices[0].generation != before.devices[0].generation);
    CHECK(snap.devices[1].generation != before.devices[1].generation);

    // Ordering: RX write, RX re-read, then TX writes.
    const size_t rxWrite = sim.indexOf("write:EC:1");
    const size_t txWrite = sim.indexOf("write:EE:1");
    CHECK(rxWrite < sim.events.size() && txWrite < sim.events.size());
    CHECK(rxWrite < txWrite);
    CHECK(sim.indexOf("write:EE:2") > txWrite);
}

static void testWriteThatDoesNotTake() {
    Sim sim;
    sim.client.beginLoad();
    CHECK(sim.run());
    sim.tx.ignoreWrites = true;
    CHECK(sim.client.applyWrites({{CRSF_ADDRESS_TX_MODULE, {writeReq(1, 3)}}}));
    CHECK(sim.run());
    const auto snap = sim.client.snapshot();
    CHECK(snap.writesUnconfirmed);
    CHECK(contains(snap.message, "Not confirmed"));
    CHECK(contains(snap.message, "Packet Rate"));
    CHECK(snap.devices[0].fields[0].value == 1);   // still what the device really holds
    CHECK(snap.devices[0].status == Status::Ready);
}

static void testDeviceGoesSilentAfterWrite() {
    Sim sim;
    sim.client.beginLoad();
    CHECK(sim.run());
    CHECK(sim.client.applyWrites({{CRSF_ADDRESS_TX_MODULE, {writeReq(1, 2)}}}));
    sim.tx.alive = false;   // e.g. the module reboots after the change
    CHECK(sim.run());
    const auto snap = sim.client.snapshot();
    CHECK(snap.devices[0].status == Status::NoResponse);
    CHECK(snap.writesUnconfirmed);   // never left "pending" forever
    CHECK(!snap.busy);
}

static void testArmedDiscardsWrites() {
    Sim sim;
    sim.client.beginLoad();
    CHECK(sim.run());
    CHECK(sim.client.applyWrites({{CRSF_ADDRESS_TX_MODULE, {writeReq(1, 2)}}}));

    sim.writesAllowed = false;   // vehicle arms before the write is sent
    CHECK(sim.run());
    CHECK(sim.firstIndexOfPrefix("write:") == sim.events.size());   // nothing was written
    const auto snap = sim.client.snapshot();
    CHECK(snap.devices[0].fields[0].value == 1);
    CHECK(snap.writesUnconfirmed);                  // an "OK" must not silently close over this
    CHECK(contains(snap.message, "armed"));
    CHECK(snap.devices[0].status == Status::Ready); // read-only reload still completes
}

static void testCommandConfirmAndCancel() {
    for (const bool confirm : {true, false}) {
        Sim sim;
        sim.client.beginLoad();
        CHECK(sim.run());

        CHECK(sim.client.runCommand(CRSF_ADDRESS_TX_MODULE, 5));
        CHECK(sim.client.snapshot().busy);
        CHECK(sim.run(4000, /*stopOnConfirm=*/true));
        auto snap = sim.client.snapshot();
        CHECK(snap.confirmation.pending);
        CHECK(snap.confirmation.fieldName == "Bind");
        CHECK(snap.confirmation.prompt == "Bind now?");
        CHECK(snap.confirmation.address == CRSF_ADDRESS_TX_MODULE && snap.confirmation.fieldId == 5);
        CHECK(snap.busy);   // nothing else may start while waiting for the operator

        sim.client.answerConfirmation(confirm);
        CHECK(!sim.client.snapshot().confirmation.pending);
        CHECK(sim.run());
        snap = sim.client.snapshot();
        CHECK(!snap.busy && !snap.confirmation.pending);
        CHECK(contains(snap.message, "Bind"));

        const size_t start = sim.indexOf("cmd:EE:5:1");
        const size_t answer = sim.indexOf(confirm ? "cmd:EE:5:4" : "cmd:EE:5:5");
        CHECK(start < sim.events.size() && answer < sim.events.size() && start < answer);
        CHECK(sim.indexOf(confirm ? "cmd:EE:5:5" : "cmd:EE:5:4") == sim.events.size());
    }
}

static void testCommandNeedingNoConfirmation() {
    Sim sim;
    sim.tx.commandNeedsConfirm = false;
    sim.client.beginLoad();
    CHECK(sim.run());
    CHECK(sim.client.runCommand(CRSF_ADDRESS_TX_MODULE, 5));
    CHECK(sim.run());
    const auto snap = sim.client.snapshot();
    CHECK(!snap.busy && !snap.confirmation.pending);
    CHECK(sim.indexOf("cmd:EE:5:1") < sim.events.size());
    CHECK(sim.indexOf("cmd:EE:5:4") == sim.events.size());   // no confirm step needed
}

static void testResetForgetsEverything() {
    Sim sim;
    sim.client.beginLoad();
    CHECK(sim.run());
    CHECK(sim.client.applyWrites({{CRSF_ADDRESS_TX_MODULE, {writeReq(1, 2)}}}));
    const uint32_t revision = sim.client.revision();

    sim.client.reset();
    const auto snap = sim.client.snapshot();
    CHECK(sim.client.revision() != revision);
    CHECK(!snap.busy);
    for (const auto& d : snap.devices) {
        CHECK(d.status == Status::Idle && d.fields.empty() && d.generation == 0);
    }
    CHECK(snap.message.empty() && !snap.writesUnconfirmed);
    CHECK(!sim.client.nextFrame(sim.now, true).has_value());   // queued write is gone
}

static void testOneFramePerCall() {
    Sim sim;
    sim.client.beginLoad();
    // However much is queued, nextFrame() never hands out more than one frame,
    // and nothing is due until the previous request is answered or times out.
    auto first = sim.client.nextFrame(sim.now, true);
    CHECK(first.has_value());
    CHECK(!sim.client.nextFrame(sim.now, true).has_value());
    CHECK(!sim.client.nextFrame(sim.now + Ms(100), true).has_value());
    auto retry = sim.client.nextFrame(sim.now + Ms(400), true);   // TX timeout is 300 ms
    CHECK(retry.has_value() && *retry == *first);
}

int main() {
    testFullLoad();
    testTinyChunks();
    testRetryAfterDroppedReplies();
    testIgnoresUnexpectedReplies();
    testDeadReceiver();
    testDeadTxModule();
    testRefusesUntilReady();
    testApplyWritesAndVerify();
    testWriteThatDoesNotTake();
    testDeviceGoesSilentAfterWrite();
    testArmedDiscardsWrites();
    testCommandConfirmAndCancel();
    testCommandNeedingNoConfirmation();
    testResetForgetsEverything();
    testOneFramePerCall();

    if (g_failures == 0) {
        std::printf("radio_param_client_test: all checks passed\n");
        return 0;
    }
    std::printf("radio_param_client_test: %d check(s) FAILED\n", g_failures);
    return 1;
}
