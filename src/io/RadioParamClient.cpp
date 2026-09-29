#include "io/RadioParamClient.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <utility>

namespace {

// The TX module answers over the USB UART; the receiver answers over the RF
// link (telemetry-paced), so it gets far more patience.
constexpr Ms kTxTimeout      {300};
constexpr Ms kRxTimeout      {1500};
// After a write, give the device time to apply it (a packet-rate change
// re-initialises the radio) before the next frame goes out.
constexpr Ms kTxWriteGap     {400};
constexpr Ms kRxWriteGap     {800};
constexpr Ms kCommandPollMin {100};
constexpr Ms kCommandTimeout {60000};

bool isTx(uint8_t address) { return address == CRSF_ADDRESS_TX_MODULE; }

const char* deviceLabel(uint8_t address) {
    return isTx(address) ? "TX module" : "receiver";
}

} // namespace

RadioParamClient::RadioParamClient() {
    std::lock_guard<std::mutex> lk(m_mutex);
    clearLocked();
}

// ---- Static helpers --------------------------------------------------------

int RadioParamClient::deviceIndex(uint8_t address) {
    for (size_t i = 0; i < kAddresses.size(); ++i)
        if (kAddresses[i] == address) return static_cast<int>(i);
    return -1;
}

RadioParamClient::Step RadioParamClient::readStep(uint8_t address, uint8_t fieldId,
                                                  uint8_t chunk, bool loadRead) {
    Step s;
    s.kind     = Step::Kind::ReadField;
    s.address  = address;
    s.fieldId  = fieldId;
    s.chunk    = chunk;
    s.loadRead = loadRead;
    return s;
}

// ---- Locked helpers --------------------------------------------------------

RadioParamClient::DeviceSnapshot* RadioParamClient::deviceLocked(uint8_t address) {
    const int idx = deviceIndex(address);
    return idx < 0 ? nullptr : &m_devices[static_cast<size_t>(idx)];
}

bool RadioParamClient::busyLocked() const {
    return m_inflight.has_value() || !m_queue.empty() || m_activeCommand.has_value();
}

void RadioParamClient::clearLocked() {
    m_queue.clear();
    m_inflight.reset();
    m_chunkBuf.clear();
    m_lastRemaining = 0xFF;
    m_consecutiveFailures = 0;
    m_holdoffUntil = {};
    m_expected.clear();
    m_unconfirmedNames.clear();
    m_activeCommand.reset();
    m_commandDeadline = {};
    m_confirmation = {};
    m_message.clear();
    for (size_t i = 0; i < m_devices.size(); ++i) {
        m_devices[i] = DeviceSnapshot{};
        m_devices[i].address = kAddresses[i];
    }
}

void RadioParamClient::eraseStepsLocked(uint8_t address) {
    m_queue.erase(std::remove_if(m_queue.begin(), m_queue.end(),
                                 [address](const Step& s) { return s.address == address; }),
                  m_queue.end());
}

std::string RadioParamClient::fieldNameLocked(uint8_t address, uint8_t fieldId) {
    const DeviceSnapshot* dev = deviceLocked(address);
    const size_t idx = static_cast<size_t>(fieldId) - 1u;
    if (dev != nullptr && fieldId != 0 && idx < dev->fields.size() &&
        !dev->fields[idx].name.empty())
        return dev->fields[idx].name;
    return "#" + std::to_string(fieldId);
}

void RadioParamClient::noteUnconfirmedLocked(uint8_t address, uint8_t fieldId) {
    if (!m_unconfirmedNames.empty()) m_unconfirmedNames += ", ";
    m_unconfirmedNames += fieldNameLocked(address, fieldId);
}

// Writes whose read-back will never happen (the device stopped answering):
// report them as unconfirmed instead of leaving them pending forever.
void RadioParamClient::abandonExpectedLocked(uint8_t address, bool allDevices) {
    for (auto it = m_expected.begin(); it != m_expected.end();) {
        if (!allDevices && it->address != address) { ++it; continue; }
        noteUnconfirmedLocked(it->address, it->fieldId);
        it = m_expected.erase(it);
    }
}

void RadioParamClient::dropWritesLocked() {
    const auto isWrite = [](const Step& s) { return s.kind == Step::Kind::WriteField; };
    if (std::none_of(m_queue.begin(), m_queue.end(), isWrite)) return;

    m_queue.erase(std::remove_if(m_queue.begin(), m_queue.end(), isWrite), m_queue.end());
    m_expected.clear();
    // Not an error message, but it keeps a pending "OK" from closing the dialog
    // over a half-applied change set.
    m_unconfirmedNames = "pending changes (vehicle armed)";
    m_activeCommand.reset();
    m_confirmation = {};
    m_message = "Pending changes cancelled: the vehicle is armed.";
    spdlog::warn("RadioParams: discarded queued writes because the vehicle is armed");
    bumpLocked();
}

void RadioParamClient::startReloadLocked(uint8_t address) {
    DeviceSnapshot* dev = deviceLocked(address);
    if (dev == nullptr) return;
    dev->status = DeviceStatus::Loading;
    dev->loaded = 0;
    dev->generation = 0;
    if (dev->fields.empty()) {
        finishLoadLocked(*dev);
        return;
    }
    // Front of the queue, ascending, so the device finishes before anything else runs.
    for (size_t id = dev->fields.size(); id >= 1; --id)
        m_queue.push_front(readStep(address, static_cast<uint8_t>(id), 0, true));
    bumpLocked();
}

void RadioParamClient::finishLoadLocked(DeviceSnapshot& dev) {
    dev.status = DeviceStatus::Ready;
    dev.generation = ++m_generation;

    // Full parameter dump: the field names/options differ between ELRS
    // versions and hardware, so this is what the bench validation reads.
    spdlog::info("RadioParams: {} '{}' ready, {} parameter(s)", deviceLabel(dev.address),
                 dev.info.name, dev.fields.size());
    for (const auto& f : dev.fields)
        spdlog::info("RadioParams:   {}", crsf::describeField(f));

    // Confirm any writes made to this device actually stuck.
    bool verified = false;
    for (auto it = m_expected.begin(); it != m_expected.end();) {
        if (it->address != dev.address) { ++it; continue; }
        verified = true;
        const size_t idx = static_cast<size_t>(it->fieldId) - 1u;
        if (it->fieldId == 0 || idx >= dev.fields.size() ||
            crsf::currentValueBytes(dev.fields[idx]) != it->value)
            noteUnconfirmedLocked(dev.address, it->fieldId);
        it = m_expected.erase(it);
    }
    // Report once every device that was written to has been re-read.
    if (verified && m_expected.empty()) {
        m_message = m_unconfirmedNames.empty()
                        ? "Settings applied and confirmed by the device."
                        : "Not confirmed by the device: " + m_unconfirmedNames;
        spdlog::info("RadioParams: {}", m_message);
    }
    bumpLocked();
}

void RadioParamClient::failInflightLocked() {
    InFlight fl = std::move(*m_inflight);
    m_inflight.reset();
    const uint8_t address = fl.step.address;
    DeviceSnapshot* dev = deviceLocked(address);
    if (dev == nullptr) return;

    bool giveUp = false;
    if (fl.step.kind == Step::Kind::Ping) {
        spdlog::warn("RadioParams: {} did not answer the device ping", deviceLabel(address));
        giveUp = true;
    } else {
        ++m_consecutiveFailures;
        spdlog::warn("RadioParams: {} did not answer read of field {} (chunk {}), failure {}",
                     deviceLabel(address), static_cast<int>(fl.step.fieldId),
                     static_cast<int>(fl.step.chunk), m_consecutiveFailures);
        giveUp = m_consecutiveFailures >= kMaxConsecutiveFailures;
    }

    if (giveUp) {
        dev->status = DeviceStatus::NoResponse;
        dev->generation = 0;
        m_message = isTx(address)
            ? "The TX module is not answering parameter requests on this port."
            : "The receiver is not answering (is it powered and linked with telemetry on?).";
        // Whatever was written but can no longer be read back is unconfirmed.
        abandonExpectedLocked(address, isTx(address));
        if (isTx(address)) {
            // The receiver is only reachable through the TX module.
            m_queue.clear();
            for (auto& d : m_devices)
                if (d.status != DeviceStatus::Ready) d.status = DeviceStatus::NoResponse;
        } else {
            eraseStepsLocked(address);
        }
        if (m_activeCommand && m_activeCommand->address == address) {
            m_activeCommand.reset();
            m_confirmation = {};
        }
    } else if (fl.step.loadRead) {
        // One unreadable field must not sink the whole load: skip it.
        m_chunkBuf.clear();
        ++dev->loaded;
        if (dev->loaded >= dev->fields.size()) finishLoadLocked(*dev);
    } else if (m_activeCommand && m_activeCommand->address == address) {
        m_activeCommand.reset();
        m_message = "No answer while polling the command status.";
    }
    bumpLocked();
}

void RadioParamClient::queueCommandWriteLocked(uint8_t address, uint8_t fieldId,
                                               crsf::CommandStatus status) {
    Step write;
    write.kind    = Step::Kind::WriteField;
    write.address = address;
    write.fieldId = fieldId;
    write.value   = crsf::encodeCommand(status);
    m_queue.push_back(std::move(write));
    // Poll the field afterwards to see the command's status.
    m_queue.push_back(readStep(address, fieldId, 0, false));
}

void RadioParamClient::handleCommandStatusLocked(uint8_t address, const crsf::Field& field) {
    if (!m_activeCommand || m_activeCommand->address != address ||
        m_activeCommand->fieldId != field.id)
        return;
    if (field.type != crsf::FieldType::Command) {
        m_activeCommand.reset();
        return;
    }

    switch (field.status) {
        case crsf::CommandStatus::Ready:
            // The info text is usually the last prompt / progress line, not a
            // result, so it goes to the log rather than the status strip.
            m_message = field.name + " finished.";
            spdlog::info("RadioParams: command '{}' finished ({})", field.name, field.text);
            m_activeCommand.reset();
            break;
        case crsf::CommandStatus::ConfirmationNeeded:
            m_confirmation = { true, address, field.id, field.name, field.text };
            break;
        default: {
            // Start / Progress / anything else: still running, poll again.
            if (Clock::now() > m_commandDeadline) {
                m_message = field.name + ": timed out waiting for the device.";
                m_activeCommand.reset();
                break;
            }
            m_message = field.name + (field.text.empty() ? "..." : ": " + field.text);
            Step poll = readStep(address, field.id, 0, false);
            poll.notBefore = Clock::now() +
                std::max(kCommandPollMin, Ms(static_cast<long long>(field.timeout10ms) * 10));
            m_queue.push_front(std::move(poll));
            break;
        }
    }
}

void RadioParamClient::onDeviceInfoLocked(const Step& step, const uint8_t* body, size_t len) {
    crsf::DeviceInfo info;
    if (!crsf::decodeDeviceInfo(body, len, info)) return;   // garbage: let the timeout retry
    DeviceSnapshot* dev = deviceLocked(step.address);
    if (dev == nullptr) return;

    m_inflight.reset();
    m_consecutiveFailures = 0;
    dev->info = info;
    dev->fields.assign(info.fieldCount, crsf::Field{});
    for (size_t i = 0; i < dev->fields.size(); ++i)
        dev->fields[i].id = static_cast<uint8_t>(i + 1);
    dev->loaded = 0;
    spdlog::info("RadioParams: {} answered as '{}', {} parameter(s), protocol v{}",
                 deviceLabel(step.address), info.name,
                 static_cast<int>(info.fieldCount), static_cast<int>(info.protocolVersion));

    if (info.fieldCount == 0) {
        finishLoadLocked(*dev);
    } else {
        dev->status = DeviceStatus::Loading;
        // Front of the queue: this device finishes before the next ping runs.
        for (int id = info.fieldCount; id >= 1; --id)
            m_queue.push_front(readStep(step.address, static_cast<uint8_t>(id), 0, true));
    }
    bumpLocked();
}

void RadioParamClient::onEntryLocked(const Step& step, const uint8_t* body, size_t len) {
    crsf::EntryChunk chunk;
    if (!crsf::decodeEntryChunk(body, len, chunk)) return;
    if (chunk.fieldId != step.fieldId) return;   // reply to something we no longer wait for
    if (step.chunk == 0) {
        m_chunkBuf.clear();
        m_lastRemaining = 0xFF;
    } else if (chunk.chunksRemaining >= m_lastRemaining) {
        return;                                  // duplicate reply to an earlier attempt
    }
    DeviceSnapshot* dev = deviceLocked(step.address);
    if (dev == nullptr) return;

    m_chunkBuf.insert(m_chunkBuf.end(), chunk.data.begin(), chunk.data.end());
    m_lastRemaining = chunk.chunksRemaining;
    m_inflight.reset();
    m_consecutiveFailures = 0;

    if (chunk.chunksRemaining > 0) {
        m_queue.push_front(readStep(step.address, step.fieldId,
                                    static_cast<uint8_t>(step.chunk + 1), step.loadRead));
        return;
    }

    crsf::Field field;
    if (!crsf::decodeField(step.fieldId, m_chunkBuf.data(), m_chunkBuf.size(), field)) {
        spdlog::warn("RadioParams: {} field {} could not be decoded ({} bytes)",
                     deviceLabel(step.address), static_cast<int>(step.fieldId),
                     m_chunkBuf.size());
        field = crsf::Field{};
        field.id     = step.fieldId;
        field.name   = "(unreadable #" + std::to_string(step.fieldId) + ")";
        field.type   = crsf::FieldType::Unsupported;
        field.loaded = true;
    }
    m_chunkBuf.clear();

    const size_t idx = static_cast<size_t>(step.fieldId) - 1u;
    if (step.fieldId >= 1 && idx < dev->fields.size()) dev->fields[idx] = field;

    handleCommandStatusLocked(step.address, field);

    if (step.loadRead) {
        ++dev->loaded;
        if (dev->loaded >= dev->fields.size()) finishLoadLocked(*dev);
    }
    bumpLocked();
}

// ---- UI thread -------------------------------------------------------------

void RadioParamClient::beginLoad() {
    std::lock_guard<std::mutex> lk(m_mutex);
    clearLocked();
    for (size_t i = 0; i < kAddresses.size(); ++i) {
        m_devices[i].status = DeviceStatus::Pinging;
        Step ping;
        ping.kind    = Step::Kind::Ping;
        ping.address = kAddresses[i];
        m_queue.push_back(std::move(ping));
    }
    bumpLocked();
}

bool RadioParamClient::applyWrites(const std::vector<DeviceWrites>& batches) {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (batches.empty() || busyLocked()) return false;
    for (const auto& batch : batches) {
        DeviceSnapshot* dev = deviceLocked(batch.address);
        if (dev == nullptr || dev->status != DeviceStatus::Ready || batch.writes.empty())
            return false;
    }

    m_unconfirmedNames.clear();
    size_t total = 0;
    for (const auto& batch : batches) {
        for (const auto& w : batch.writes) {
            Step s;
            s.kind    = Step::Kind::WriteField;
            s.address = batch.address;
            s.fieldId = w.fieldId;
            s.value   = w.value;
            m_queue.push_back(std::move(s));
            m_expected.push_back({ batch.address, w.fieldId, w.value });
            ++total;
        }
        Step reload;
        reload.kind    = Step::Kind::ReloadAll;
        reload.address = batch.address;
        m_queue.push_back(std::move(reload));
    }

    m_message = "Applying " + std::to_string(total) + " change(s)...";
    bumpLocked();
    return true;
}

bool RadioParamClient::runCommand(uint8_t address, uint8_t fieldId) {
    std::lock_guard<std::mutex> lk(m_mutex);
    DeviceSnapshot* dev = deviceLocked(address);
    if (dev == nullptr || dev->status != DeviceStatus::Ready || busyLocked()) return false;
    const size_t idx = static_cast<size_t>(fieldId) - 1u;
    if (fieldId == 0 || idx >= dev->fields.size() ||
        dev->fields[idx].type != crsf::FieldType::Command)
        return false;

    m_activeCommand   = ActiveCommand{ address, fieldId };
    m_commandDeadline = Clock::now() + kCommandTimeout;
    m_confirmation    = {};
    m_message         = dev->fields[idx].name + "...";
    queueCommandWriteLocked(address, fieldId, crsf::CommandStatus::Start);
    bumpLocked();
    return true;
}

void RadioParamClient::answerConfirmation(bool confirm) {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_confirmation.pending || !m_activeCommand) return;
    const ActiveCommand cmd = *m_activeCommand;
    m_confirmation = {};
    queueCommandWriteLocked(cmd.address, cmd.fieldId,
                            confirm ? crsf::CommandStatus::Confirm
                                    : crsf::CommandStatus::Cancel);
    bumpLocked();
}

void RadioParamClient::reset() {
    std::lock_guard<std::mutex> lk(m_mutex);
    clearLocked();
    bumpLocked();
}

RadioParamClient::Snapshot RadioParamClient::snapshot() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    Snapshot s;
    s.revision     = m_revision.load();
    s.busy         = busyLocked();
    s.devices      = m_devices;
    s.confirmation = m_confirmation;
    s.message      = m_message;
    s.writesUnconfirmed = !m_unconfirmedNames.empty();
    return s;
}

// ---- Worker thread ---------------------------------------------------------

void RadioParamClient::onFrame(uint8_t type, uint8_t origin, const uint8_t* body, size_t len) {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_inflight || m_inflight->step.address != origin) return;

    const Step step = m_inflight->step;   // copy: the handlers reset m_inflight
    if (type == CRSF_FRAMETYPE_DEVICE_INFO && step.kind == Step::Kind::Ping)
        onDeviceInfoLocked(step, body, len);
    else if (type == CRSF_FRAMETYPE_PARAM_ENTRY && step.kind == Step::Kind::ReadField)
        onEntryLocked(step, body, len);
}

std::optional<std::vector<uint8_t>> RadioParamClient::nextFrame(Clock::time_point now,
                                                                bool writesAllowed) {
    std::lock_guard<std::mutex> lk(m_mutex);

    if (!writesAllowed) dropWritesLocked();

    if (m_inflight) {
        if (now - m_inflight->sentAt < m_inflight->timeout) return std::nullopt;
        if (m_inflight->attempts < kMaxAttempts) {
            ++m_inflight->attempts;
            m_inflight->sentAt = now;
            return m_inflight->frame;
        }
        failInflightLocked();
    }

    if (now < m_holdoffUntil) return std::nullopt;

    while (!m_queue.empty()) {
        if (now < m_queue.front().notBefore) return std::nullopt;
        Step step = std::move(m_queue.front());
        m_queue.pop_front();

        std::vector<uint8_t> frame;
        switch (step.kind) {
            case Step::Kind::ReloadAll:
                startReloadLocked(step.address);
                continue;
            case Step::Kind::Ping:
                frame = crsf::buildPing(step.address);
                break;
            case Step::Kind::ReadField:
                frame = crsf::buildParamRead(step.address, step.fieldId, step.chunk);
                break;
            case Step::Kind::WriteField:
                frame = crsf::buildParamWrite(step.address, step.fieldId, step.value);
                break;
        }
        if (frame.empty()) {
            spdlog::warn("RadioParams: dropped an unencodable request for field {}",
                         static_cast<int>(step.fieldId));
            continue;
        }

        if (step.kind == Step::Kind::WriteField) {
            // No reply is expected; the read-back after the last write verifies.
            m_holdoffUntil = now + (isTx(step.address) ? kTxWriteGap : kRxWriteGap);
            return frame;
        }

        InFlight fl;
        fl.step     = std::move(step);
        fl.frame    = frame;
        fl.sentAt   = now;
        fl.timeout  = isTx(fl.step.address) ? kTxTimeout : kRxTimeout;
        fl.attempts = 1;
        m_inflight  = std::move(fl);
        return frame;
    }
    return std::nullopt;
}
