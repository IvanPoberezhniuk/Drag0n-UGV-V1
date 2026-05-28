#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <timeapi.h>
#include "App.h"
#include "crsf/CrsfPacket.h"
#include "input/KeyboardInput.h"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>
#include <algorithm>

using Clock = std::chrono::steady_clock;
using Ms    = std::chrono::milliseconds;
using Us    = std::chrono::microseconds;

static App* g_appInstance = nullptr;

static BOOL WINAPI ctrlHandler(DWORD) {
    if (g_appInstance) {
        spdlog::info("Shutdown signal received — stopping...");
        g_appInstance->requestStop();
    }
    return TRUE;
}

App::App(AppConfig config)
    : m_config(std::move(config))
    , m_input(std::make_unique<KeyboardInput>())
{}

void App::requestStop() {
    m_running = false;
}

static uint16_t mapAxis(float v) {
    // v in [-1.0, +1.0] → [CH_MIN, CH_MAX] with 0 → CH_CENTER
    float clamped = std::max(-1.0f, std::min(1.0f, v));
    if (clamped >= 0.0f)
        return static_cast<uint16_t>(CH_CENTER + clamped * (CH_MAX - CH_CENTER));
    else
        return static_cast<uint16_t>(CH_CENTER + clamped * (CH_CENTER - CH_MIN));
}

RcChannels App::mapStateToChannels(const ControlState& state) const {
    RcChannels ch;
    for (auto& c : ch.ch) c = CH_CENTER;

    const auto& idx = m_config.channels;
    ch.ch[idx.steering - 1] = mapAxis(state.steering);
    ch.ch[idx.throttle - 1] = mapAxis(state.throttle);

    // Drive mode: 1→MIN, 2→CENTER, 3→MAX
    if (state.driveMode == 1)      ch.ch[idx.mode - 1] = CH_MIN;
    else if (state.driveMode == 2) ch.ch[idx.mode - 1] = CH_CENTER;
    else                           ch.ch[idx.mode - 1] = CH_MAX;

    ch.ch[idx.lights - 1] = state.lightsOn ? CH_MAX : CH_MIN;
    ch.ch[idx.arm   - 1]  = state.armed    ? CH_MAX : CH_MIN;
    ch.ch[idx.estop - 1]  = state.emergencyStop ? CH_MAX : CH_MIN;

    return ch;
}

void App::applyFailsafe(ControlState& state) {
    auto now = Clock::now();
    auto msSinceInput = std::chrono::duration_cast<Ms>(now - state.lastUpdated).count();

    // Input timeout failsafe
    if (msSinceInput > static_cast<long long>(m_config.failsafeTimeoutMs)) {
        if (!m_inFailsafe) {
            spdlog::warn("Failsafe: no input for {}ms — holding neutral", msSinceInput);
            m_inFailsafe = true;
        }
        state.throttle = 0.0f;
        state.steering = 0.0f;
    } else {
        m_inFailsafe = false;
    }

    // Armed interlock: never allow throttle unless armed
    if (!state.armed) {
        state.throttle = 0.0f;
    }

    // Emergency stop overrides everything (latched — cleared only when re-armed)
    if (state.emergencyStop) {
        state.throttle = 0.0f;
        state.steering = 0.0f;
        state.armed    = false;
    }
}

int App::run() {
    // Improve Windows timer resolution to ~1ms for accurate 50Hz loop
    timeBeginPeriod(1);

    g_appInstance = this;
    SetConsoleCtrlHandler(ctrlHandler, TRUE);

    std::string port = m_config.serialPort;
    if (port == "auto") {
        port = SerialPort::autoDetect();
        if (port.empty()) {
            spdlog::error("Auto-detect: no ELRS TX module found. Running in dry-run mode.");
        }
    }

    if (!port.empty()) {
        spdlog::info("Opening serial port {} at {} baud...", port, m_config.baudrate);
        if (!m_serial.open(port, m_config.baudrate)) {
            spdlog::error("Failed to open serial port. Running in dry-run mode (no serial output).");
        }
    }

    const auto intervalUs = Us(1'000'000 / m_config.controlRateHz);
    uint64_t frameCount = 0;
    auto statsTimer = Clock::now();

    spdlog::info("Control loop started at {}Hz. Press Enter to ARM.", m_config.controlRateHz);
    spdlog::info("  W/S=throttle  A/D=steering  Space=ESTOP  L=lights  1/2/3=mode");

    while (m_running) {
        auto frameStart = Clock::now();

        m_input->poll();
        ControlState state = m_input->getState();
        applyFailsafe(state);

        RcChannels channels = mapStateToChannels(state);
        auto packet = buildRcChannelsPacket(channels);

        if (m_serial.isOpen()) {
            m_serial.write(packet.data(), packet.size());
        }

        ++frameCount;

        // Log stats once per second
        auto elapsed = std::chrono::duration_cast<Ms>(Clock::now() - statsTimer).count();
        if (elapsed >= 1000) {
            spdlog::debug("Stats: {}pkt/s | armed={} estop={} thr={:.2f} str={:.2f}",
                frameCount, state.armed, state.emergencyStop,
                state.throttle, state.steering);
            frameCount = 0;
            statsTimer = Clock::now();
        }

        // Sleep for the remainder of the frame interval
        auto frameDuration = Clock::now() - frameStart;
        if (frameDuration < intervalUs)
            std::this_thread::sleep_for(intervalUs - frameDuration);
    }

    // Send one safe packet before exiting
    if (m_serial.isOpen()) {
        RcChannels safe{};
        for (auto& c : safe.ch) c = CH_CENTER;
        safe.ch[m_config.channels.arm   - 1] = CH_MIN;
        safe.ch[m_config.channels.estop - 1] = CH_MAX;
        auto pkt = buildRcChannelsPacket(safe);
        m_serial.write(pkt.data(), pkt.size());
        m_serial.close();
    }

    timeEndPeriod(1);
    spdlog::info("connectionApp stopped cleanly.");
    return 0;
}
