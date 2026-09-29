#pragma once
#include "core/ChronoTypes.h"
#include "crsf/CrsfParams.h"
#include "crsf/CrsfTypes.h"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Request/response state machine for the CRSF device-parameter protocol: it
// enumerates the ExpressLRS TX module's and the receiver's settings (the same
// menu an EdgeTX handset shows through the ELRS Lua script), and writes edits
// back. Wire encoding lives in crsf/CrsfParams.
//
// Threading, following the rest of the app ("worker writes, UI polls"):
//   * worker thread (SerialWorker): onFrame() with parsed replies, nextFrame()
//     to fetch the next frame to send. nextFrame() hands out AT MOST ONE frame
//     per call and SerialWorker calls it once per 50 Hz loop iteration, after
//     the RC frame, so parameter traffic can never delay or replace RC frames.
//   * UI thread (RadioSettingsDialog): beginLoad(), applyWrites(),
//     runCommand(), answerConfirmation(), reset(), snapshot().
// Every method takes the internal mutex; nothing here touches the serial port.
//
// Failure behaviour: every request is retried, then the device is marked
// NoResponse and its remaining requests are dropped -- no retry storm, and the
// RC loop is unaffected. Writes are only ever sent while the caller reports
// writesAllowed (vehicle disarmed); queued writes are discarded otherwise.
class RadioParamClient {
public:
    // Parameters are read from these two addresses, in this order.
    static constexpr size_t kDeviceCount = 2;
    static constexpr std::array<uint8_t, kDeviceCount> kAddresses = {
        CRSF_ADDRESS_TX_MODULE, CRSF_ADDRESS_RECEIVER };

    enum class DeviceStatus {
        Idle,          // nothing requested yet
        Pinging,       // waiting for the device to identify itself
        Loading,       // reading its parameters
        Ready,         // parameter tree complete
        NoResponse,    // gave up: not answering
    };

    struct DeviceSnapshot {
        uint8_t          address = 0;
        DeviceStatus     status  = DeviceStatus::Idle;
        crsf::DeviceInfo info;
        std::vector<crsf::Field> fields;   // index == field id - 1
        size_t           loaded  = 0;      // fields resolved so far (read or skipped)
        // Changes every time a full (re)load completes; 0 while not Ready.
        uint32_t         generation = 0;
    };

    // A command (Bind, WiFi update, ...) asked the operator to confirm.
    struct Confirmation {
        bool        pending   = false;
        uint8_t     address   = 0;
        uint8_t     fieldId   = 0;
        std::string fieldName;
        std::string prompt;
    };

    struct Snapshot {
        uint32_t revision = 0;
        bool     busy     = false;   // requests in flight / queued, or a command running
        std::array<DeviceSnapshot, kDeviceCount> devices;
        Confirmation confirmation;
        std::string  message;        // last progress / error line for the status strip
        // The last applyWrites() finished but the device still reports an old
        // value for at least one field.
        bool         writesUnconfirmed = false;
    };

    struct WriteRequest {
        uint8_t              fieldId = 0;
        std::vector<uint8_t> value;  // wire bytes, see crsf::encode*()
    };

    struct DeviceWrites {
        uint8_t                   address = 0;
        std::vector<WriteRequest> writes;
    };

    RadioParamClient();

    // ---- UI thread ---------------------------------------------------------

    // (Re)starts enumeration of both devices, discarding what was known.
    void beginLoad();
    // Queues, per device and in the given order, its writes followed by a full
    // re-read of that device, which also verifies the values took effect.
    // Put the receiver before the TX module: receiver writes travel over the RF
    // link, which a TX-module change (packet rate, ...) may interrupt.
    // Returns false (and queues nothing) if any device is not Ready or requests
    // are still outstanding.
    bool applyWrites(const std::vector<DeviceWrites>& batches);
    // Starts a Command field. Same refusal rules as applyWrites().
    bool runCommand(uint8_t address, uint8_t fieldId);
    void answerConfirmation(bool confirm);
    // Drops everything queued and forgets all device state (dialog closed,
    // serial connect / disconnect / error).
    void reset();

    uint32_t revision() const { return m_revision.load(); }
    Snapshot snapshot() const;

    // ---- Worker thread -----------------------------------------------------

    // A device-info / parameter-entry reply from the parser.
    void onFrame(uint8_t type, uint8_t origin, const uint8_t* body, size_t len);
    // Next frame to write to the port, if any is due. Write / command frames
    // are held back -- and discarded -- while !writesAllowed.
    std::optional<std::vector<uint8_t>> nextFrame(Clock::time_point now, bool writesAllowed);

private:
    struct Step {
        enum class Kind { Ping, ReadField, WriteField, ReloadAll } kind = Kind::Ping;
        uint8_t  address  = 0;
        uint8_t  fieldId  = 0;
        uint8_t  chunk    = 0;
        bool     loadRead = false;            // counts towards the device's load progress
        std::vector<uint8_t> value;           // WriteField payload
        Clock::time_point notBefore{};
    };

    struct InFlight {
        Step                 step;
        std::vector<uint8_t> frame;
        Clock::time_point    sentAt{};
        Ms                   timeout{0};
        int                  attempts = 0;
    };

    struct ExpectedWrite {
        uint8_t              address = 0;
        uint8_t              fieldId = 0;
        std::vector<uint8_t> value;
    };

    struct ActiveCommand {
        uint8_t address = 0;
        uint8_t fieldId = 0;
    };

    static constexpr int kMaxAttempts            = 3;
    static constexpr int kMaxConsecutiveFailures = 3;

    static int  deviceIndex(uint8_t address);
    static Step readStep(uint8_t address, uint8_t fieldId, uint8_t chunk, bool loadRead);

    // *Locked: caller holds m_mutex.
    DeviceSnapshot* deviceLocked(uint8_t address);
    void bumpLocked() { m_revision.fetch_add(1); }
    bool busyLocked() const;
    void clearLocked();
    void dropWritesLocked();
    void eraseStepsLocked(uint8_t address);
    std::string fieldNameLocked(uint8_t address, uint8_t fieldId);
    void noteUnconfirmedLocked(uint8_t address, uint8_t fieldId);
    void abandonExpectedLocked(uint8_t address, bool allDevices);
    void startReloadLocked(uint8_t address);
    void finishLoadLocked(DeviceSnapshot& dev);
    void failInflightLocked();
    void onDeviceInfoLocked(const Step& step, const uint8_t* body, size_t len);
    void onEntryLocked(const Step& step, const uint8_t* body, size_t len);
    void handleCommandStatusLocked(uint8_t address, const crsf::Field& field);
    void queueCommandWriteLocked(uint8_t address, uint8_t fieldId, crsf::CommandStatus status);

    mutable std::mutex m_mutex;
    std::atomic<uint32_t> m_revision{0};

    std::array<DeviceSnapshot, kDeviceCount> m_devices;
    std::deque<Step>          m_queue;
    std::optional<InFlight>   m_inflight;
    std::vector<uint8_t>      m_chunkBuf;          // chunks of the field being read
    uint8_t                   m_lastRemaining = 0xFF;
    int                       m_consecutiveFailures = 0;
    Clock::time_point         m_holdoffUntil{};    // settle time after a write
    std::vector<ExpectedWrite> m_expected;
    std::string               m_unconfirmedNames;  // fields whose write did not read back
    std::optional<ActiveCommand> m_activeCommand;
    Clock::time_point         m_commandDeadline{};
    Confirmation              m_confirmation;
    std::string               m_message;
    uint32_t                  m_generation = 0;
};
