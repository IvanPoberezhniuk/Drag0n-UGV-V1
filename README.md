# UGV Control Station

Qt6 desktop app for controlling an unmanned ground vehicle over CRSF/serial (ELRS TX module).

## Requirements

- [MSYS2](https://www.msys2.org/) with the UCRT64 toolchain
- [CMake](https://cmake.org/) 3.21+ and [Ninja](https://ninja-build.org/)

Install Qt6 and the MinGW toolchain via MSYS2 (one-time):

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc \
          mingw-w64-ucrt-x86_64-qt6-base \
          mingw-w64-ucrt-x86_64-qt6-declarative \
          mingw-w64-ucrt-x86_64-ninja \
          mingw-w64-ucrt-x86_64-ffmpeg \
          mingw-w64-ucrt-x86_64-pkgconf
```

`spdlog`, `nlohmann_json`, and `entt` are fetched automatically by CMake
(`FetchContent`) — no separate install step.

The live camera panel (`VideoPanel`/`VideoWorker`) decodes the Raspberry
Pi's RTSP feed via raw FFmpeg (libavformat/avcodec/swscale), found through
`pkg-config` — the `mingw-w64-ucrt-x86_64-ffmpeg` and
`mingw-w64-ucrt-x86_64-pkgconf` packages above are required to configure
the project, not optional extras.

---

## Build

`build/` is the one and only build directory for this project — configure
and build into it, don't create `build_cmake/`, `build_mingw/`,
`build_verify/`, or any other variant. If `build/` is in a broken state,
delete and reconfigure it rather than starting a new directory.

Qt's host tools (rcc, moc, qmlimportscanner, ...) need their DLLs on `PATH`
at configure and build time, so put `C:\msys64\ucrt64\bin` on `PATH` first.

### Configure (first time or after changing CMakeLists.txt)

```bash
cmake -G Ninja -B build ^
  -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64 ^
  -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe ^
  -DCMAKE_MAKE_PROGRAM=C:/msys64/ucrt64/bin/ninja.exe ^
  -DCMAKE_BUILD_TYPE=Release
```

### Build

```bash
cmake --build build
```

### Debug build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Clean build artifacts

```bash
cmake --build build --target clean
```

---

## Run

```bash
build\UGVControlStation.exe
```

The Logs panel displays live UGV diagnostic telemetry relayed from ESP32
through XR4 and Nomad. The same entries are persisted beside the executable in
`build\ugv-control.log` (1 MB rotating file with three backups), so the
latest diagnostic session can be shared without screenshots.

### With a specific config file

```bash
build\UGVControlStation.exe --config path\to\config.json
```

---

## Deploy (copy Qt DLLs next to the exe)

```bash
windeployqt6 build\UGVControlStation.exe
```

After this the exe can be run directly without the MSYS2 `bin` on `PATH`.

---

## IntelliSense (VS Code)

`compile_commands.json` is generated automatically in `build/` on every
configure (`CMAKE_EXPORT_COMPILE_COMMANDS=ON` is set in `CMakeLists.txt`), and
`.vscode/c_cpp_properties.json` already points there.

---

## Config file

Place `config.json` next to the exe or pass `--config <path>`. All fields are optional — omit any to use the default.

```json
{
  "serial": {
    "port": "auto",
    "baudrate": 400000
  },
  "control": {
    "rateHz": 50,
    "failsafeTimeoutMs": 300
  },
  "channels": {
    "steering": 1,
    "throttle": 2,
    "mode":     3,
    "lights":   4,
    "arm":      5,
    "estop":    6
  },
  "ui": {
    "fontSize": 10
  }
}
```

`"port": "auto"` auto-detects the first connected ELRS TX module (CP210x, CH340, CH341, STM32 VCP, FTDI).

---

## Keyboard controls

| Key | Action |
|-----|--------|
| `W` / `S` | Throttle forward / reverse |
| `A` / `D` | Steer left / right |
| `Enter` | Arm / disarm toggle (also clears ESTOP latch) |
| `Space` | Emergency stop (latches — re-arm to clear) |
| `L` | Toggle lights |
| `1` / `2` / `3` | Drive mode |

Keyboard and gamepad controls are accepted only while a window belonging to
the control station is in the foreground. Switching to another application
immediately zeros throttle and steering; inputs made there are ignored.

When controlling a RadioMaster Nomad directly through its built-in USB-UART
without a radio handset, the app sends a one-shot ELRS bind command after every
serial connection. This starts the stock firmware's RF scheduler; the module
then returns to normal operation using its saved Binding UID. The motor command
stream remains disarmed and neutral during this startup.

---

## Radio settings (gear button next to Connect)

With no handset, the ExpressLRS menu an EdgeTX radio shows through its Lua script
(Packet Rate, Telem Ratio, TX Power, Switch Mode, Antenna Mode, Bind, WiFi, ... and
the receiver's own options) is available from the gear button beside Connect. It
opens a Preferences-style popup with a **TX module / Receiver** selector, a sidebar
of the device's folders, and Apply / OK / Cancel. The pages are built from what the
device itself reports, so they follow the installed ELRS version.

- Enabled only while connected. Editing and applying need the vehicle **disarmed**;
  queued writes are discarded if it arms.
- Edits are staged until Apply / OK; each write is re-read from the device and
  anything it did not accept is reported.
- Changes that can drop the link (packet rate, switch mode, antenna mode, WiFi,
  model match, receiver protocol) ask for confirmation. Receiver failsafe options
  that would replay the last channel values are disabled.
- The full parameter list of each device is written to the log when it finishes
  loading; use it to check the field names the safety guards match on.
- **Unverified on hardware:** that the Nomad answers parameter requests on its USB
  port, and that the receiver is reachable through it. If the popup shows "not
  answering", the ELRS WiFi web UI or Configurator (which need the COM port closed)
  remain the fallback.

Protocol tests (Qt-free, no hardware): configure with `-DUGV_BUILD_TESTS=ON`, build the
`crsf_params_test` (frame codec) and `radio_param_client_test` (request state machine
against a simulated TX module and receiver) targets and run them; exit code 0 means
all checks passed.

---

## Project structure

```
src/
├── main.cpp                  — app entry point, single-instance guard, dark theme
├── core/                     — ECS state components (entt)
│   ├── AppState.h            — central registry + mutex
│   ├── ControlState.h        — throttle, steering, arm, estop, lights, drive mode
│   ├── TelemetryState.h      — RSSI, LQ, battery voltage
│   ├── SafetyState.h         — failsafe + estop latch flags
│   ├── ConnectionState.h     — port name, status, pkt/s
│   └── LogBuffer.h           — thread-safe circular log + spdlog sink
├── crsf/                     — CRSF protocol
│   ├── CrsfTypes.h           — frame type constants, RcChannels struct
│   ├── CrsfPacket.cpp/h      — RC_CHANNELS_PACKED encoder, CRC8 DVB-S2
│   └── CrsfParams.cpp/h      — device-parameter codec (ping/info/entry/read/write)
├── io/                       — serial communication
│   ├── SerialPort.cpp/h      — Win32 COM port wrapper, auto-detect, enumeration
│   ├── SerialWorker.cpp/h    — worker thread: send loop, RX parser, reconnect
│   └── RadioParamClient.cpp/h — ELRS TX/RX settings request state machine
├── input/
│   ├── KeyboardInput.cpp/h   — Win32 GetAsyncKeyState polling
│   └── GamepadInput.cpp/h    — SDL2 gamepad (optional)
├── services/
│   ├── SafetyService.cpp/h   — failsafe timeout, armed interlock, ESTOP latch
│   └── ControlService.cpp/h  — ControlState → RcChannels channel mapping
├── config/
│   └── AppConfig.cpp/h       — JSON config load/defaults
└── ui/
    ├── MainWindow.cpp/h      — QMainWindow, dock layout, 30 ms poll timer
    ├── SettingsDialog.cpp/h  — Preferences (UI / Controls / Connection)
    ├── RadioSettingsDialog.cpp/h — ELRS TX module / receiver settings popup
    └── panels/
        ├── ConnectionPanel   — port selector, connect/disconnect, status
        ├── ControlPanel      — throttle/steering bars, arm, estop, drive mode
        ├── TelemetryPanel    — RSSI, LQ, battery (live when RX sends frames)
        ├── LogsPanel         — filterable real-time log viewer
        └── LegendPanel       — keyboard & Xbox controller visual reference
```
